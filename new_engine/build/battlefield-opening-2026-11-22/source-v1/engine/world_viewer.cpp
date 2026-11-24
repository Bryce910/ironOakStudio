#include "engine/world_currency.h"
#ifndef _WIN32
#include <dlfcn.h>
#endif
#include "engine/world_player_inventory.h"
#include "engine/world_player_profile.h"
#include "engine/world_npc_nameplate.h"
#include "engine/world_scenery.h"
#include "engine/world_scenery_pivot.h"
#include "engine/world_opening_interior.h"
#include "engine/world_studio.h"
#include "engine/world_apartment_window_light.h"
#include "engine/world_editor_scene_guard.h"
#include "engine/world_structure_collision.h"
#include "engine/world_cover_collision.h"
#include "engine/world_pond_field.h"
#include "engine/world_studio_height_changes.h"
#include "engine/world_height_region.h"
#include "engine/world_json_string.h"
#include "engine/world_playtest_slice.h"
#include "engine/world_playtest_walk.h"
#include "engine/world_opening_prologue.h"
#include "engine/world_battlefield_prologue.h"
#include "engine/world_battlefield_actors.h"
#include "engine/world_battlefield_scene.h"
#include "engine/world_battlefield_audio.h"
#include "engine/world_opening_dialogue_camera.h"
#include "engine/world_journey_presentation.h"
#include "engine/world_studio_scene_record.h"
#include "engine/world_opening_game_screen.h"
#include "engine/world_opening_cinematic_settings.h"
#include "engine/world_opening_cinematic_audio.h"
#include "engine/world_npc_intro_media.h"
#include "engine/world_grandstone_qa_cases.h"
#include "engine/world_transform_math.h"
#include "engine/world_landscape_protection.h"
#include "engine/world_designer_viewport.h"
#include "engine/world_designer.h"
#include "engine/world_designer_assets.h"
#include "engine/world_designer_workspace.h"
#include "engine/world_designer_scene_index.h"
#include "engine/world_editor_picking.h"
#include "engine/world_surface_images.h"
#include "engine/world_surface_import.h"
#include "engine/world_surface_field.h"
#include "engine/world_authoring_identity.h"
#include "engine/world_prefab.h"
#include "engine/world_editor_session.h"
#include "engine/world_project_checkpoint.h"
#include "engine/world_production_jobs.h"
#include "engine/world_living.h"
#include "engine/world_micro_policy.h"
#include "engine/world_npc_memory.h"
#include "engine/world_npc_appearance.h"
#include "engine/world_terrain_authoring.h"
#include "engine/world_cinematics.h"
#include "engine/world_cinematics_audio.h"
#include "engine/world_cutscenes.h"
#include "engine/world_cutscenes_host.h"
extern "C" {
#include "engine/audio.h"
}
#include "engine/world_regions.h"
#include "engine/world_gameplay.h"
#include "engine/world_physics.h"
#include "engine/world_city_scene.h"
#include "engine/world_city_sky.h"
#include "engine/world_animation.h"
#include "engine/world_runtime_components.h"
#include "engine/world_runtime_demo.h"
#include "engine/world_runtime_animation.h"
#include <ctime>
#include <limits>
#include "engine/world_navigation.h"
#include "engine/world_npc_authoring.h"
#include "engine/world_studio_tools.h"
#include "engine/world_assets.h"
#include "engine/world_environment.h"
#include <future>
#include <functional>
#include <filesystem>
#include <memory>
#include <chrono>
#include <set>
#include <climits>
#include <cfloat>
#include "engine/world_demo_contours.h"
#include "engine/world_combat_motion.h"
#include "engine/world_orc_roster.h"
#include "engine/world_knight_roster.h"
#include "engine/world_arena_appearance_assignment.h"
#include "engine/world_bow_presentation.h"
#include "engine/world_skeletal_hero_loadouts.h"
#include "engine/world_armor_wear.h"
#include "engine/world_skeletal_hero_viewmodel.h"
#include "engine/world_neck_blood.h"
#include "engine/world_sever_blood.h"
#include "engine/world_arena_combat.h"
#include "engine/world_arena_camera.h"
#include "engine/world_arena_render.h"
#include "engine/world_arena_bow_sockets.inc"
#include "engine/world_combat_vfx_render.h"
#include "engine/world_combat_feedback.h"
#include "engine/world_arena_progress.h"
#include "engine/world_blood_render.h"
#include "engine/world_presentation_voice.h"
#include "engine/world_castle_dragons.h"
#include "engine/world_castle_dragons_layout.h"
#include "engine/world_dragon_breath.h"
#include "engine/world_dragon_lights.h"
#include "engine/world_follow_camera.h"
#include "engine/world_follow_camera_safety.h"
#include "engine/world_exploration_camera.h"
#include "engine/world_character_presentation.h"
#include "engine/world_character_terminal.h"
#include "engine/world_combat_shoulder_camera.h"
#include "engine/world_combat_closeup_camera.h"
#include "engine/world_combat_focus_camera.h"
#include "engine/world_presentation.h"
#include "engine/world_controller.h"
#include "engine/world_player_movement.h"
#include "engine/world_arena_player_movement.h"
#include "engine/world_combat_stick.h"
#include "engine/world_controller_qa.h"
#include "engine/world_controller_ui.h"
#include "engine/world_source_review_route.h"
#include "engine/world_presentation_audio.h"
#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_opengl_glext.h>
extern "C" {
#include "engine/world_viewer.h"
#include "engine/world_platform.h"
#include "engine/world_atlas.h"
#include "engine/world_tiles.h"
#include "engine/world_walk.h"
#include "engine/texture_cache.h"
#include "engine/app_bootstrap.h"
#include "engine/performance.h"
#include "engine/audio.h"
}

#include "engine/world_models.h"
#include "engine/world_joust_render.h"
#include "engine/world_joust_impacts.h"
#include "engine/world_joust_view.h"
#include "engine/world_model_cpu.h"
#include "engine/world_asset_thumbnail_math.h"
#include "engine/world_cover.h"
#include "engine/world_grass_transition.h"
#include "engine/world_temporal.h"
#include "engine/world_currency.h"
#include "engine/world_player_inventory.h"
#include "engine/world_npc_nameplate.h"
#include "engine/world_scenery.h"
#include "engine/world_water_scene.h"
#include "engine/world_sky.h"
#include "engine/world_occlusion.h"
#include "engine/world_upscale.h"
#include "engine/world_ambience.h"
#include "engine/world_bloom.h"
#include "engine/world_auto_exposure.h"
#include "engine/world_object_motion.h"
#include "engine/world_weather.h"
#include "engine/world_storm_lighting.h"
#include "engine/world_rainforest_atmosphere.h"
#include "engine/world_milestone.h"
#include "engine/world_jungle_expansion_data.h"
#include "engine/world_stronghold.h"
#include "engine/world_stronghold_data.h"
#include "engine/world_nature_grandstone.h"
#include "engine/world_snow.h"
#include "engine/world_forest_shelter.h"
#include "engine/world_pool_reflection_scissor.h"
#include "engine/world_river_reflection_scissor.h"
#include "engine/world_water_mirror_cache.h"
#include "engine/world_pool_reflection_visibility.h"
#include "engine/world_water_reflection_demand.h"
#include "engine/world_grass_interaction.h"
#include "engine/world_minimap.h"
#include "engine/world_swimming.h"
#include "engine/world_aquatic_effects.h"
#include "engine/world_selection_membership.h"
#include "engine/world_horizon_grid.h"
#include "engine/world_terrain_visibility.h"
#include "engine/world_height_region.h"
#include "engine/world_scene_particles.h"
#include "engine/world_vfx.h"
#include "engine/world_demo_audio.h"
#include "engine/world_storm_controller.h"
#include "engine/world_weather_transition.h"
#include "engine/world_uniform_cache.h"
#include "engine/world_local_probes.h"
#include "engine/world_gpu_budget_audit.h"
#include "engine/world_texture_pool_policy.h"
#include "engine/world_village_population.h"
#include "engine/world_road_traffic.h"
#include "engine/world_castle_training.h"
#include "engine/world_training_combat.h"
#include "engine/world_village_watch.h"
#include "engine/world_wildlife.h"
#include "engine/world_bandit_camp.h"
#include "engine/world_trail_camp.h"
#include "engine/world_graphics_settings.h"
#include "engine/world_character_textures.h"
#include "engine/world_character_quality.h"
#include "engine/world_ground_cover_quality.h"
#include "engine/world_grass_density.h"
#include "engine/world_quality_controller.h"
#include "engine/world_castle_creek.h"
#include "engine/world_castle_visibility.h"
#include "engine/world_market_hero_look.h"
#include "engine/world_village_pets.h"
#include "engine/world_shadow_math.h"
#include "engine/world_village_sky.h"
#include "engine/world_village_textures.h"
#include "engine/world_castle_lights.h"
#include "engine/world_tavern_lighting.h"
#include "engine/world_torch_lights.h"
#include "engine/world_village_torches.inc"
#include "engine/world_bandit_torches.inc"
#include "engine/world_bandit_route.inc"
#include "engine/world_castle_streets_torches.inc"
#include "engine/world_castle_streets_route.inc"
#include "engine/world_village_farms.h"
#include "stb_image.h"
#include "engine/world_showcase_terrain.h"
#include "engine/world_color_grade.h"
#include "gl_helpers.h"
#include "third_party/imgui/imgui.h"
#include "engine/world_demo_presentation.h"
#include "third_party/imgui/backends/imgui_impl_sdl2.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <string>
#include <fstream>
#include <deque>
#include <sstream>
#include <iomanip>
#include <tuple>
#include <utility>
#include <unistd.h>
#include "engine/world_weather_drips.inc"

namespace {
    bool visual_surface_finish=false;std::array<float,4> visual_grass_interactor={};
    world_grass_interaction::State grass_interaction;
    uint64_t grass_player_movement_queries=0,grass_player_movement_query_failures=0;
    bool milestone_finish=false;
    bool stronghold_finish=false;
    bool source_showcase=false;
    bool landscape_authoring_session=false;
    bool city_showcase=false;
    world_city_scene::Look city_look;
    const WorldCitySky *active_city_sky=nullptr;
    const world_physics::World *active_city_physics=nullptr;
    const world_playtest_walk::Support *active_playtest_walk=nullptr;
    bool active_structure_world=false;
    double playtest_walk_reference=0;
    struct V3 {
        float x, y, z;
    };

    std::string combat_reaction_physics_fields(const WorldCombatAnatomyStats &s) {
        std::ostringstream out;out.imbue(std::locale::classic());out<<std::setprecision(17)
            <<"\"substeps\":"<<s.physics_substeps<<",\"contacts\":"<<s.physics_contacts<<",\"sleeps\":"<<s.physics_sleeps
            <<",\"budget_clamps\":"<<s.physics_budget_clamps<<",\"invalid\":"<<s.physics_invalid
            <<",\"support_points_active\":"<<s.support_points_active<<",\"maximum_ground_penetration\":"<<s.maximum_ground_penetration
            <<",\"maximum_update_us\":"<<s.physics_update_us_max;
        return out.str();
    }
    std::string combat_reaction_death_fields(const WorldCombatDeathStats &s) {
        std::ostringstream out;out.imbue(std::locale::classic());out<<std::setprecision(17)
            <<"\"loaded\":"<<(s.loaded?"true":"false")<<",\"captures\":"<<s.captures<<",\"frames\":"<<s.frames
            <<",\"completions\":"<<s.completions<<",\"body_draws\":"<<s.body_draws<<",\"cap_draws\":"<<s.cap_draws
            <<",\"crowd_draws\":"<<s.crowd_draws<<",\"failures\":"<<s.failures<<",\"source_cpu_bytes\":"<<s.source_cpu_bytes
            <<",\"staging_cpu_bytes\":"<<s.staging_cpu_bytes<<",\"extra_gpu_bytes\":"<<s.extra_gpu_bytes<<",\"maximum_update_ms\":"<<s.maximum_update_ms;
        return out.str();
    }

    // Opt-in visual QA only. Capture consecutive, unmodified display pixels
    // after the temporal resolve. Never enable readbacks during FPS profiles.
    struct FoliageCaptureSequence {
        const char *prefix = std::getenv("VALWOOD_FOLIAGE_SEQUENCE");
        const bool full_frame = std::getenv("VALWOOD_FOLIAGE_SEQUENCE_FULL") != nullptr;
        const int samples=32;
        std::vector<unsigned char> pixels;
        void capture(uint64_t frame, int end_frame, int width, int height) {
            if(!prefix||!*prefix||end_frame<samples+120||frame<=uint64_t(end_frame-samples))return;
            const int w=full_frame?width:std::min(width,960),h=full_frame?height:std::min(height,360);
            const int x=(width-w)/2,y=full_frame?0:std::min(30,height-h);
            pixels.resize(size_t(w)*h*3);
            GLint alignment=4;glGetIntegerv(GL_PACK_ALIGNMENT,&alignment);
            glPixelStorei(GL_PACK_ALIGNMENT,1);glReadBuffer(GL_BACK);
            glReadPixels(x,y,w,h,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());
            glPixelStorei(GL_PACK_ALIGNMENT,alignment);
            const size_t stride=size_t(w)*3;
            for(int row=0;row<h/2;row++)
                std::swap_ranges(pixels.data()+row*stride,pixels.data()+(row+1)*stride,pixels.data()+(h-1-row)*stride);
            SDL_Surface *surface=SDL_CreateRGBSurfaceWithFormatFrom(pixels.data(),w,h,24,int(stride),SDL_PIXELFORMAT_RGB24);
            char path[1400];int n=std::snprintf(path,sizeof(path),"%s-%03llu.bmp",prefix,
                static_cast<unsigned long long>(frame-(end_frame-samples)-1));
            if(!surface||n<0||size_t(n)>=sizeof(path)||SDL_SaveBMP(surface,path)!=0)
                std::fprintf(stderr,"[Foliage QA] Sequence capture failed at frame %llu\n",static_cast<unsigned long long>(frame));
            if(surface)SDL_FreeSurface(surface);
            if(frame==uint64_t(end_frame))std::printf("[Display QA] Captured%d consecutive %dx%d display pixels, offset(%d,%d), prefix %s\n",samples,w,h,x,y,prefix);
        }
    };

    bool capture_source_review(world_source_review::Route &route,int width,int height) {
        if(!route.pending_checkpoint())return true;
        const std::string path=route.capture_path();
        if(path.empty()){route.acknowledge_checkpoint(true,false);return true;}
        std::vector<unsigned char> pixels(std::size_t(width)*height*3);
        GLint alignment=4;glGetIntegerv(GL_PACK_ALIGNMENT,&alignment);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadBuffer(GL_BACK);
        glReadPixels(0,0,width,height,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());glPixelStorei(GL_PACK_ALIGNMENT,alignment);
        const std::size_t stride=std::size_t(width)*3;
        for(int row=0;row<height/2;row++)std::swap_ranges(pixels.data()+row*stride,pixels.data()+(row+1)*stride,pixels.data()+(height-1-row)*stride);
        SDL_Surface *surface=SDL_CreateRGBSurfaceWithFormatFrom(pixels.data(),width,height,24,int(stride),SDL_PIXELFORMAT_RGB24);
        const bool ok=surface&&SDL_SaveBMP(surface,path.c_str())==0;if(surface)SDL_FreeSurface(surface);
        route.acknowledge_checkpoint(ok,true);
        std::printf("[Source review] %s %s\n",ok?"Captured":"FAILED",path.c_str());return ok;
    }

    V3 add(V3 a, V3 b) {
        return {a.x + b.x, a.y + b.y, a.z + b.z};
    }

    V3 mul(V3 a, float b) {
        return {a.x * b, a.y * b, a.z * b};
    }

    float dot(V3 a, V3 b) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    V3 cross(V3 a, V3 b) {
        return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
    }

    V3 norm(V3 a) {
        return mul(a, 1.f / std::max(std::sqrt(dot(a, a)), .00001f));
    }

    struct M4 {
        float m[16];
    };

    M4 identity() {
        M4 r = {};
        r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1;
        return r;
    }

    M4 product(M4 a, M4 b) {
        M4 r = {};
        for (int c = 0; c < 4; c++)
            for (int row = 0; row < 4; row++)
                for (int k = 0; k < 4; k++)
                    r.m[c * 4 + row] += a.m[k * 4 + row] * b.m[c * 4 + k];
        return r;
    }

    M4 perspective(float fov, float aspect, float near, float far) {
        M4 r = {};
        float f = 1 / std::tan(fov * .5f);
        r.m[0] = f / aspect;
        r.m[5] = f;
        r.m[10] = (far + near) / (near - far);
        r.m[11] = -1;
        r.m[14] = 2 * far * near / (near - far);
        return r;
    }

    M4 look(V3 eye, V3 target, V3 up) {
        V3 f = norm(add(target, mul(eye, -1))), s = norm(cross(f, up)), u = cross(s, f);
        M4 r = identity();
        r.m[0] = s.x;
        r.m[4] = s.y;
        r.m[8] = s.z;
        r.m[1] = u.x;
        r.m[5] = u.y;
        r.m[9] = u.z;
        r.m[2] = -f.x;
        r.m[6] = -f.y;
        r.m[10] = -f.z;
        r.m[12] = -dot(s, eye);
        r.m[13] = -dot(u, eye);
        r.m[14] = dot(f, eye);
        return r;
    }

    bool studio_shader_features=false,studio_environment_features=false,studio_water_features=false,studio_imported_features=false,studio_wind_features=false;
    world_graph::Code studio_graph_code;
    WorldEnvironment *studio_environment=nullptr;

    bool local_light_shadows_supported=false;
    struct Program {
        enum class GrassRole { None, Owner, Specialized, Vegetation, Prepass };
        GLuint id = 0;
        // A child follows its owner's move/rollback/free lifecycle. Only the
        // plain generated grass color batches may select this optional child.
        std::unique_ptr<Program> generated_grass;
        // AAA merge: trees/cover/outcrops/horizon variant without character
        // hair/skin paths (register pressure; ~7 ms at the camp on RX 5300).
        std::unique_ptr<Program> vegetation;
        std::unique_ptr<Program> depth_prepass; // AAA merge: coverage-only foliage depth variant
        WorldUniformCache loc;
        std::array<GLint,10> torch_uniforms={};
        bool torch_cached=false;
        Program()=default;
        Program(const Program &)=delete;
        Program &operator=(const Program &)=delete;
        Program(Program &&other)noexcept{*this=std::move(other);}
        Program &operator=(Program &&other)noexcept{
            if(this!=&other){free();id=std::exchange(other.id,0);loc=std::move(other.loc);torch_uniforms=other.torch_uniforms;torch_cached=other.torch_cached;generated_grass=std::move(other.generated_grass);vegetation=std::move(other.vegetation);depth_prepass=std::move(other.depth_prepass);other.loc.reset();other.torch_cached=false;}
            return *this;
        }
        GLint at(const char *n) {
            return loc.get(id,n,[&](const char *name){return glGetUniformLocation(id,name);});
        }

        bool load(const char *v, const char *f,bool reject_source=false,bool character=false,GrassRole grass_role=GrassRole::None) {
            loc.reset();
            torch_cached=false;
            // The measured scalar kernel remains the default. QA can compare
            // a separately compiled gather variant without imposing that
            // variant's register cost on ordinary play.
            const bool gather_trial=std::getenv("VALWOOD_QA_ENABLE_SHADOW_GATHER")&&
                !std::getenv("VALWOOD_QA_DISABLE_SHADOW_GATHER");
            // A generation owns its shader bank while user projects keep
            // their scene/saves and original resource aliases. Absolute QA
            // shader paths remain explicit, and includes resolve alongside
            // the selected source through the existing loader.
            const char *bank=std::getenv("VALWOOD_SHADER_BANK_ROOT");
            std::string vertex_path,fragment_path;
            if(bank&&*bank){
                if(!std::strncmp(v,"shaders/",8))vertex_path=(std::filesystem::path(bank)/(v+8)).string();
                if(!std::strncmp(f,"shaders/",8))fragment_path=(std::filesystem::path(bank)/(f+8)).string();
            }
            char *vs=glsl_load(vertex_path.empty()?v:vertex_path.c_str()),*fs=glsl_load(fragment_path.empty()?f:fragment_path.c_str());
            if(vs&&fs){
                std::string vertex(vs),fragment(fs),defines;
                if(local_light_shadows_supported)defines+="#define VALWOOD_LOCAL_LIGHT_SHADOW 1\n";
                if(character)defines+="#define VALWOOD_CHARACTER_APPEARANCE 1\n";
                if(!gather_trial&&fragment.find("VALWOOD_QA_DISABLE_TEXTURE_GATHER")!=std::string::npos)defines+="#define VALWOOD_QA_DISABLE_TEXTURE_GATHER 1\n";
                if(studio_shader_features)defines+="#define VALWOOD_STUDIO_SURFACES 1\n";
                if(studio_environment_features)defines+="#define VALWOOD_HDR_ENVIRONMENT 1\n";
                if(studio_wind_features)defines+="#define VALWOOD_LOCAL_WIND 1\n";
                if(studio_imported_features)defines+="#define VALWOOD_IMPORTED_SURFACES 1\n";
                if(studio_water_features)defines+="#define VALWOOD_STUDIO_WATER 1\n";
                if(!studio_graph_code.surface.empty()){
                    defines+="#define VALWOOD_STUDIO_GRAPH 1\n";
                    auto at=vertex.find("// VALWOOD_GRAPH_VERTEX");if(at!=std::string::npos)vertex.replace(at,std::strlen("// VALWOOD_GRAPH_VERTEX"),studio_graph_code.vertex);
                    at=fragment.find("// VALWOOD_GRAPH_SURFACE");if(at!=std::string::npos)fragment.replace(at,std::strlen("// VALWOOD_GRAPH_SURFACE"),studio_graph_code.surface);
                }
                if(reject_source)fragment+="\ninvalid_studio_qa_shader_token\n";
                const auto vl=vertex.find('\n'),fl=fragment.find('\n');
                if(vl!=std::string::npos)vertex.insert(vl+1,defines);
                if(grass_role==GrassRole::Specialized)defines+="#define VALWOOD_GENERATED_GRASS_FRAGMENT 1\n";
                if(grass_role==GrassRole::Vegetation)defines+="#define VALWOOD_VEGETATION_FRAGMENT 1\n";
                if(grass_role==GrassRole::Prepass)defines+="#define VALWOOD_DEPTH_PREPASS_FRAGMENT 1\n";
                if(fl!=std::string::npos)fragment.insert(fl+1,defines);
                id=GLH_program(vertex.c_str(),fragment.c_str());
            }
            std::free(vs);std::free(fs);
            if(!id)return false;
            GLint ok = 0;
            glGetProgramiv(id, GL_LINK_STATUS, &ok);
            if(ok&&studio_environment_features){GLint current=0;glGetIntegerv(GL_CURRENT_PROGRAM,&current);glUseProgram(id);glUniform1i(glGetUniformLocation(id,"uEnvironmentCube"),15);glUniform1i(glGetUniformLocation(id,"uEnvironmentBRDF"),6);glUseProgram(GLuint(current));}
            if(ok&&!studio_graph_code.surface.empty()){GLint current=0;glGetIntegerv(GL_CURRENT_PROGRAM,&current);glUseProgram(id);glUniform1i(glGetUniformLocation(id,"uStudioGraphIndex"),-1);glUseProgram(GLuint(current));}
            if(ok&&grass_role==GrassRole::Owner&&studio_graph_code.surface.empty()&&studio_graph_code.vertex.empty()&&
               !std::getenv("VALWOOD_QA_DISABLE_GRASS_SPECIALIZATION")){
                auto next=std::make_unique<Program>();
                if(next->load(v,f,false,false,GrassRole::Specialized))generated_grass=std::move(next);
                else{next->free();std::fprintf(stderr,"[World] Grass shader specialization unavailable; using general foliage shader\n");}
                if(!std::getenv("VALWOOD_QA_DISABLE_VEGETATION_SPECIALIZATION")){
                    auto veg=std::make_unique<Program>();
                    if(veg->load(v,f,false,false,GrassRole::Vegetation))vegetation=std::move(veg);
                    else{veg->free();std::fprintf(stderr,"[World] Vegetation shader specialization unavailable; using general foliage shader\n");}
                }
                {
                    auto pre=std::make_unique<Program>();
                    if(pre->load(v,f,false,false,GrassRole::Prepass))depth_prepass=std::move(pre);
                    else{pre->free();std::fprintf(stderr,"[World] Foliage depth prepass variant unavailable\n");}
                }
            }
            return ok != 0;
        }

        void use() {
            glUseProgram(id);
        }

        void f(const char *n, float x) {
            glUniform1f(at(n), x);
        }

        void i(const char *n, int x) {
            glUniform1i(at(n), x);
        }

        void v2(const char *n, float x, float y) {
            glUniform2f(at(n), x, y);
        }

        void v3(const char *n, V3 x) {
            glUniform3f(at(n), x.x, x.y, x.z);
        }

        void m4(const char *n, M4 x) {
            glUniformMatrix4fv(at(n), 1, GL_FALSE, x.m);
        }

        void free() {
            if(generated_grass){generated_grass->free();generated_grass.reset();}
            if(vegetation){vegetation->free();vegetation.reset();}
            if(depth_prepass){depth_prepass->free();depth_prepass.reset();}
            if(id)glDeleteProgram(id);
            id = 0;
            loc.reset();
        }
    };

    world_weather_authoring::PackedFrame active_authored_weather;
    world_weather_wetness::Packet active_weather_wetness;
    WorldStormLightFrame active_storm_light;
    #include "engine/world_lightning_visual.inc"
    #include "engine/world_asset_thumbnail_renderer.inc"
    #include "engine/world_surface_renderer.inc"

    #include "engine/world_training_combat_view.inc"

    GLuint texture(const char *path, bool srgb = true) {
        int w, h, c;
        unsigned char *p = stbi_load(path, &w, &h, &c, 4);
        if (!p) {
            std::fprintf(stderr, "[World] Missing texture %s\n", path);
            return 0;
        }

        GLuint t;
        glGenTextures(1, &t);
        glBindTexture(GL_TEXTURE_2D, t);
        glTexImage2D(GL_TEXTURE_2D, 0, srgb ? GL_SRGB8_ALPHA8 : GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, p);
        stbi_image_free(p);
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        if (SDL_GL_ExtensionSupported("GL_EXT_texture_filter_anisotropic")) {
            GLfloat maximum = 1;
            glGetFloatv(0x84FF, &maximum);
            glTexParameterf(GL_TEXTURE_2D, 0x84FE, std::min(maximum, 8.f));
        }
        return t;
    }

    struct Target {
        GLuint fbo = 0, color = 0, depth = 0, indirect = 0;
        bool separate_indirect = false;
        int w = 0, h = 0;
        void release() {
            glDeleteFramebuffers(1, &fbo);
            glDeleteTextures(1, &color);
            glDeleteTextures(1, &depth);
            glDeleteTextures(1, &indirect);
            fbo = color = depth = indirect = 0;
            w = h = 0;
        }

        bool allocate(int x, int y) {
            if(x<1||y<1||x>8192||y>8192||uint64_t(x)*uint64_t(y)>16777216ull)
                return false;
            if (x == w && y == h)
                return true;
            release();
            w = x;
            h = y;
            glGenFramebuffers(1, &fbo);
            glBindFramebuffer(GL_FRAMEBUFFER, fbo);
            glGenTextures(1, &color);
            glBindTexture(GL_TEXTURE_2D, color);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color, 0);
            glGenTextures(1, &depth);
            glBindTexture(GL_TEXTURE_2D, depth);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, w, h, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth, 0);
            if(separate_indirect) {
                glGenTextures(1,&indirect);glBindTexture(GL_TEXTURE_2D,indirect);
                glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA16F,w,h,0,GL_RGBA,GL_FLOAT,nullptr);
                glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
                glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT1,GL_TEXTURE_2D,indirect,0);
                const GLenum buffers[]={GL_COLOR_ATTACHMENT0,GL_COLOR_ATTACHMENT1};glDrawBuffers(2,buffers);
            }
            bool ok = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
            if(ok) {glClearColor(.12f,.17f,.22f,1);glClearDepth(1.);glDepthMask(GL_TRUE);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);}
            else release();
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            return ok;
        }
        bool resize(int x,int y) {
            if(x==w&&y==h&&fbo)return true;
            Target next;next.separate_indirect=separate_indirect;
            if(!next.allocate(x,y))return false;
            release();*this=next;return true;
        }
    };

    struct GpuTile {
        GLuint vao = 0, vbo = 0, ebo = 0;
        uint64_t generation = 0;
        WorldTileKey key = {};
        double x = 0, z = 0;
        float size = 0, min_y = 0, max_y = 0;
        GLsizei count = 0;
        size_t bytes = 0;
        void release() {
            glDeleteVertexArrays(1, &vao);
            glDeleteBuffers(1, &vbo);
            glDeleteBuffers(1, &ebo);
            vao = vbo = ebo = 0;
            generation = 0;
            count = 0;
            bytes = 0;
        }

        void upload(const WorldTileMesh &m) {
            if (!vao) {
                glGenVertexArrays(1, &vao);
                glGenBuffers(1, &vbo);
                glGenBuffers(1, &ebo);
            }
            glBindVertexArray(vao);
            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glBufferData(GL_ARRAY_BUFFER, m.vertex_count * sizeof(WorldTileVertex), m.vertices, GL_STATIC_DRAW);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, m.index_count * sizeof(uint32_t), m.indices, GL_STATIC_DRAW);
            for (int i = 0; i < 2; i++) {
                glEnableVertexAttribArray(i);
                glVertexAttribPointer(i, 3, GL_FLOAT, GL_FALSE, sizeof(WorldTileVertex), (void *)(uintptr_t)(i * 3 * sizeof(float)));
            }
            glBindVertexArray(0);
            generation = m.generation;
            key = m.key;
            x = m.origin_x;
            z = m.origin_z;
            size = m.size;
            min_y = m.min_y;
            max_y = m.max_y;
            count = (GLsizei) m.index_count;
            bytes = m.vertex_count * sizeof(WorldTileVertex) + m.index_count * sizeof(uint32_t);
        }
    };

    struct WorldQueries {
        struct Slot {
            GLuint start = 0, end = 0;
            bool pending = false;
        };
        std::array<Slot, 4> slots = {};
        int active = -1;
        float milliseconds = 0;
        void init() {
            for (auto &s : slots) {
                glGenQueries(1, &s.start);
                glGenQueries(1, &s.end);
            }
        }

        void begin(uint64_t frame) {
            active = -1;
            for (auto &s : slots)
                if (s.pending) {
                    GLint ready = 0;
                    glGetQueryObjectiv(s.end, GL_QUERY_RESULT_AVAILABLE, &ready);
                    if (ready) {
                        GLuint64 a, b;
                        glGetQueryObjectui64v(s.start, GL_QUERY_RESULT, &a);
                        glGetQueryObjectui64v(s.end, GL_QUERY_RESULT, &b);
                        milliseconds = float(double(b - a) * .000001);
                        s.pending = false;
                    }
                }
            auto &s = slots[size_t(frame % slots.size())];
            if (!s.pending) {
                glQueryCounter(s.start, GL_TIMESTAMP);
                active = int(frame % slots.size());
            }
        }

        void end() {
            if (active >= 0) {
                auto &s = slots[size_t(active)];
                glQueryCounter(s.end, GL_TIMESTAMP);
                s.pending = true;
                active = -1;
            }
        }

        void release() {
            for (auto &s : slots) {
                glDeleteQueries(1, &s.start);
                glDeleteQueries(1, &s.end);
            }
        }
    };

    struct Camera {
        double x = 0, y = 0, z = 0;
        float yaw = 0, pitch = -.06f;
        float aspect=16.f/9.f,fov=1.08f;
        bool fly = false;
        bool grounded = true;
        bool swimming = false;
        world_swimming::DepthState dive;
        float vy = 0;
        V3 forward() const {
            return {
                std::sin(yaw) * std::cos(pitch), std::sin(pitch), -std::cos(yaw) * std::cos(pitch)
            };
        }

        V3 right() const {
            return {std::cos(yaw), 0, std::sin(yaw)};
        }
    };

    // Editor dolly changes position rather than field of view. Use the ground
    // in front of the camera as a distance reference, without mutating the
    // selected object or the Landscape brush's last pick.
    bool editor_camera_near_interior(const Camera &camera,const world_opening_interior::Config &interior){
        if(!interior.enabled)return false;
        const double p[]={camera.x,camera.y,camera.z};
        const double margin[]={5.,6.,5.};
        for(unsigned i=0;i<3;++i)
            if(!std::isfinite(p[i])||p[i]<interior.bounds.minimum[i]-margin[i]||
               p[i]>interior.bounds.maximum[i]+margin[i])return false;
        return true;
    }
    bool editor_camera_dolly(Camera &camera,double scroll,const world_opening_interior::Config &interior) {
        if(!std::isfinite(scroll)||scroll==0)return false;
        const auto direction=camera.forward();
        // The apartment is an elevated interior near y=600. Outdoor terrain
        // lies hundreds of metres below it, so a terrain-based dolly distance
        // makes a single wheel tick throw the editor camera out of the room.
        const bool near_apartment=editor_camera_near_interior(camera,interior);
        double distance=near_apartment?
            std::clamp(std::abs(camera.y-(interior.bounds.minimum[1]+.5))+2.,3.,12.):
            std::clamp(std::abs(camera.y-world_atlas_height(camera.x,camera.z)),4.,400.);
        double previous=.25;
        if(!near_apartment&&camera.y>world_atlas_height(camera.x,camera.z)+.35){
            for(double t=.25;t<=8192.;t+=std::max(.25,t*.02)){
                const double x=camera.x+direction.x*t,z=camera.z+direction.z*t;
                if(camera.y+direction.y*t<=world_atlas_height(x,z)){
                    double lo=previous,hi=t;
                    for(unsigned i=0;i<12;++i){const double middle=(lo+hi)*.5;
                        if(camera.y+direction.y*middle>world_atlas_height(camera.x+direction.x*middle,camera.z+direction.z*middle))lo=middle;else hi=middle;
                    }
                    distance=hi;break;
                }
                previous=t;
            }
        }
        const double step=std::clamp(distance*(1.-std::exp(-std::clamp(scroll,-8.,8.)*.18)),
                                     near_apartment?-2.5:-1000.,near_apartment?2.5:1000.);
        const double forward_step=step>0?std::min(step,std::max(0.,distance-.35)):step;
        const double x=std::clamp(camera.x+direction.x*forward_step,-world_atlas_width()*.5+1.,world_atlas_width()*.5-1.);
        const double z=std::clamp(camera.z+direction.z*forward_step,-world_atlas_depth()*.5+1.,world_atlas_depth()*.5-1.);
        const double y=std::clamp(camera.y+direction.y*forward_step,-12000.,12000.);
        if(x==camera.x&&y==camera.y&&z==camera.z)return false;
        camera.x=x;camera.y=y;camera.z=z;camera.fly=true;camera.grounded=false;camera.vy=0;return true;
    }

    float sample_height(double x, double z, void *) {
        return world_atlas_height(x, z);
    }

    // A constrained imported scene uses its own geometry and reference storey;
    // an unsupported probe must never fall back to unrelated atlas terrain.
    bool active_opening_prologue=false,active_combat_arena=false,active_follow_camera=false;
    float view_near_plane(){return active_playtest_walk||active_opening_prologue||active_combat_arena||active_follow_camera?.12f:.4f;}
    float walking_height(double x, double z, void *) {
        if(active_playtest_walk){float floor=0;return active_playtest_walk->sample(x,z,playtest_walk_reference,floor)?floor:(active_structure_world?std::max(world_atlas_walk_height(x,z),0.f):float(playtest_walk_reference));}
        return std::max(world_atlas_walk_height(x, z), 0.f);
    }

    bool skeletal_foot_support(double x,double probe_y,double z,WorldSkeletalHero::Support &out,void *) {
        if(!std::isfinite(x)||!std::isfinite(probe_y)||!std::isfinite(z))return false;
        if(active_playtest_walk||active_city_physics) {
            world_physics::Hit hit;
            const world_physics::Vec3 start{x,probe_y+.30,z},down{0,-.75,0};
            const bool queried=active_playtest_walk?active_playtest_walk->raycast(start,down,hit):active_city_physics->raycast(start,down,hit);
            if(!queried)return false;
            if(hit.hit) {
                if(hit.normal.y<.65||std::abs(hit.position.y-probe_y)>.32)return false;
                out.height=hit.position.y;out.nx=float(hit.normal.x);out.ny=float(hit.normal.y);out.nz=float(hit.normal.z);
                return std::isfinite(out.height);
            }
            // A constrained imported sector has no terrain behind a missed
            // support ray. Structure-over-terrain worlds explicitly do.
            if(active_playtest_walk&&!active_structure_world)return false;
        }
        const double height=std::max(double(world_atlas_walk_height(x,z)),0.);
        if(!std::isfinite(height)||std::abs(height-probe_y)>.32)return false;
        constexpr double radius=.06;
        const auto floor=[](double px,double pz){return std::max(double(world_atlas_walk_height(px,pz)),0.);};
        const double dx=(floor(x-radius,z)-floor(x+radius,z))/(2*radius);
        const double dz=(floor(x,z-radius)-floor(x,z+radius))/(2*radius);
        const double length=std::sqrt(dx*dx+1+dz*dz);
        if(!std::isfinite(length)||1/length<.65)return false;
        out.height=height;out.nx=float(dx/length);out.ny=float(1/length);out.nz=float(dz/length);return true;
    }

    bool camera_swimming(const Camera &cam,bool water_controls_allowed) {
        if(cam.fly||!water_controls_allowed||active_combat_arena)return false;
        float surface=0,depth=0;
        const bool water=world_atlas_local_water(cam.x,cam.z,&surface,&depth);
        return world_swimming::active(cam.swimming,water,cam.y,surface,walking_height(cam.x,cam.z,nullptr));
    }

    void update_camera_swimming(Camera &cam,bool water_controls_allowed) {
        const bool next=camera_swimming(cam,water_controls_allowed);
        if(next!=cam.swimming){
            float surface=0,depth=0;world_atlas_local_water(cam.x,cam.z,&surface,&depth);
            std::printf("[Swimming] %s at %.3f %.3f %.3f; surface %.3f support %.3f\n",next?"entered":"left",cam.x,cam.y,cam.z,surface,walking_height(cam.x,cam.z,nullptr));
        }
        cam.swimming=next;
        if(!next)cam.dive={};
    }

    struct WorldLiveVisuals;
    bool world_live_actor_move_clear(const WorldLiveVisuals*,const world_runtime::Runtime*,double,double,double,double,float,float,float);
    const WorldLiveVisuals *active_live_visuals=nullptr;
    WorldRoadTraffic *active_road_traffic=nullptr;
    const WorldStronghold *active_stronghold=nullptr;
    world_runtime::Runtime *active_world_runtime=nullptr;
    world_cover_collision::Field generated_cover_collision;
    bool generated_cover_collision_ready=true;
    std::array<bool,2> generated_cover_kind_ready={true,true};
    struct WalkingMotion {
        double attempted = 0, actual = 0;
        bool collision = false, slope = false, sliding = false;
    };
    WalkingMotion move_camera(Camera &cam,const WorldScenery *scene,double nx,double nz) {
        WalkingMotion result;
        const double ox=cam.x,oz=cam.z;
        result.attempted=std::hypot(nx-ox,nz-oz);
        if(result.attempted<1.e-10) return result;
        if(cam.fly) {cam.x=nx;cam.z=nz;result.actual=result.attempted;return result;}
        float feet=float(cam.y-1.75),approved_floor=feet;
        const float original_feet=feet;
        if(active_playtest_walk)playtest_walk_reference=feet;
        auto terrain_clear=[&](double ax,double az,double bx,double bz) {
            // The authored mesh service checks real stairs, floors and their
            // headroom. A height-field slope test would reject discrete steps.
            if(cam.swimming)return walking_height(bx,bz,nullptr)-feet<=world_playtest_walk::max_step+.002;
            if(active_playtest_walk)return true;
            return world_walk_can_move(ax,az,bx,bz,walking_height,nullptr,1.2);
        };
        auto scenery_clear=[&](double ax,double az,double bx,double bz) {
            if(active_playtest_walk&&cam.swimming){
                // A riverbed can be many metres below the floating capsule.
                // Keep the actual body sweep, without requiring a nearby tread.
                ++grass_player_movement_queries;
                if(walking_height(bx,bz,nullptr)-feet>world_playtest_walk::max_step+.002)return false;
                approved_floor=feet;
                world_physics::Hit hit;
                if(!active_playtest_walk->cast_capsule_movement({ax,feet+.9,az},.37f,.50f,{bx-ax,0,bz-az},hit))return false;
                if(hit.hit&&hit.fraction<.999f){
                    // A floating body cannot ignore an upward contact. Only a
                    // genuine grounded shore landing may use the normal full
                    // step/headroom casts; every other contact still blocks.
                    float current_floor=0,next_floor=0;
                    if(hit.normal.y<.65f||!active_playtest_walk->sample(ax,az,feet,current_floor)||
                       std::abs(feet-current_floor)>.15||
                       !active_playtest_walk->can_move(ax,az,bx,bz,feet,next_floor))return false;
                    approved_floor=next_floor;
                }
            }else if(active_playtest_walk){
                ++grass_player_movement_queries;
                float old_source=0,next_source=0;
                const bool old_supported=active_structure_world&&active_playtest_walk->sample(ax,az,feet,old_source);
                const bool next_supported=active_structure_world&&active_playtest_walk->sample(bx,bz,feet,next_source);
                if(active_structure_world&&(!old_supported||!next_supported)){
                    approved_floor=next_supported?next_source:std::max(world_atlas_walk_height(bx,bz),0.f);
                    if(cam.grounded&&(approved_floor-feet>world_playtest_walk::max_step+.002||feet-approved_floor>world_playtest_walk::max_drop+.002))return false;
                    if(!old_supported&&!next_supported&&!world_walk_can_move(ax,az,bx,bz,walking_height,nullptr,1.2))return false;
                    world_physics::Hit hit;
                    const double dy=cam.grounded?double(approved_floor)-feet:0;
                    if(!active_playtest_walk->cast_capsule_movement({ax,feet+.9,az},.37f,.50f,{bx-ax,dy,bz-az},hit)||(hit.hit&&hit.fraction<.999f&&hit.normal.y<.65f))return false;
                }else if(cam.grounded){
                    if(!active_playtest_walk->can_move(ax,az,bx,bz,feet,approved_floor))return false;
                }else{
                    // Airborne motion keeps the real body height. Floor step
                    // limits describe walking and must not stop a jump whose
                    // capsule already clears the riser. Retain the bounded
                    // source-floor guard and sweep at the actual feet.
                    if(!active_playtest_walk->contains(ax,az)||
                       !active_playtest_walk->sample(bx,bz,feet,approved_floor))return false;
                    world_physics::Hit hit;
                    if(!active_playtest_walk->cast_capsule_movement({ax,feet+.9,az},.37f,.50f,{bx-ax,0,bz-az},hit)||
                       (hit.hit&&hit.fraction<.999f))return false;
                }
            }
            if(!world_scenery_can_move(scene,ax,az,bx,bz,feet,1.75f,.37f))return false;
            if(!generated_cover_collision_ready||!generated_cover_collision.can_move(ax,az,bx,bz,feet,1.75,.37))return false;
            if(!world_stronghold_can_move(active_stronghold,ax,az,bx,bz,feet,.37f))return false;
            if(!world_live_actor_move_clear(active_live_visuals,active_world_runtime,ax,az,bx,bz,feet,1.75f,.37f))return false;
            if(world_road_traffic_blocks_segment(active_road_traffic,ax,az,bx,bz,feet,1.75,.37))return false;
            if(active_world_runtime&&active_world_runtime->configured()){world_physics::Hit hit;world_runtime::Id entity=0;if(active_world_runtime->cast_capsule({ax,feet+.9,az},.37f,.50f,{bx-ax,0,bz-az},hit,entity)&&hit.hit&&hit.normal.y<.65&&hit.fraction<.999f)return false;}
            if(active_city_physics){world_physics::Hit hit;++grass_player_movement_queries;const bool queried=active_city_physics->cast_capsule({ax,feet+.9,az},.37f,.50f,{bx-ax,0,bz-az},hit);if(!queried)++grass_player_movement_query_failures;if(queried&&hit.hit&&hit.normal.y<.65&&hit.fraction<.999f)return false;}
            return true;
        };
        if(active_playtest_walk){
            // Each accepted axis owns its approved floor. Reusing the old feet
            // for the second axis creates false collisions beside real steps.
            auto attempt=[&](double tx,double tz){
                if(!scenery_clear(cam.x,cam.z,tx,tz))return false;
                cam.x=tx;cam.z=tz;
                if(cam.grounded||(cam.swimming&&approved_floor>feet)){
                    if(cam.swimming&&approved_floor>feet)cam.grounded=true;
                    feet=approved_floor;cam.y=feet+1.75;
                }
                playtest_walk_reference=feet;return true;
            };
            if(!attempt(nx,nz)){
                result.collision=true;
                attempt(nx,cam.z);
                const Camera x_only=cam;const float x_feet=feet;
                attempt(cam.x,nz);
                // Two independently accepted sliding axes must not compose
                // into a climb/drop beyond one ordinary stair step.
                if(cam.grounded&&(feet-original_feet>world_playtest_walk::max_step+.002||
                                  original_feet-feet>world_playtest_walk::max_drop+.002)){
                    cam=x_only;feet=x_feet;playtest_walk_reference=feet;
                }
                result.sliding=cam.x!=ox||cam.z!=oz;
            }
            result.actual=std::hypot(cam.x-ox,cam.z-oz);return result;
        }
        result.slope=!terrain_clear(ox,oz,nx,nz);
        result.collision=!scenery_clear(ox,oz,nx,nz);
        if(!result.slope&&!result.collision) {cam.x=nx;cam.z=nz;}
        else {
            // Retry each swept axis for wall sliding. Every retry retains the
            // same terrain and capsule checks as the requested diagonal move.
            if(terrain_clear(cam.x,cam.z,nx,cam.z)&&scenery_clear(cam.x,cam.z,nx,cam.z))cam.x=nx;
            const double x_only=cam.x;
            if(terrain_clear(cam.x,cam.z,cam.x,nz)&&scenery_clear(cam.x,cam.z,cam.x,nz))cam.z=nz;
            // Axis retries must not bypass the maximum climb by composing two
            // individually legal uphill steps into a too-steep diagonal.
            if(!terrain_clear(ox,oz,cam.x,cam.z)) {cam.x=x_only;cam.z=oz;}
            result.sliding=cam.x!=ox||cam.z!=oz;
        }
        result.actual=std::hypot(cam.x-ox,cam.z-oz);
        if(active_playtest_walk&&cam.grounded){float floor=0;if(active_playtest_walk->sample(cam.x,cam.z,feet,floor)){cam.y=floor+1.75;playtest_walk_reference=floor;}}
        return result;
    }
    void move_camera_vertical(Camera &cam,const WorldScenery *scene,double requested,double floor) {
        requested=std::max(requested,floor);
        cam.grounded=requested<=floor;
        if(active_playtest_walk&&std::abs(requested-cam.y)>1.e-8){
            world_physics::Hit hit;
            const bool queried=active_playtest_walk->cast_capsule_movement({cam.x,cam.y-1.75+.9,cam.z},.37f,.50f,{0,requested-cam.y,0},hit);
            if(!queried){requested=cam.y;cam.vy=0;}
            else if(hit.hit&&hit.fraction<.999f){
                const bool descending=requested<cam.y;
                requested=cam.y+(requested-cam.y)*std::max(0.f,hit.fraction-.002f);
                cam.grounded=descending&&hit.normal.y>.65;cam.vy=0;
            }
        }
        if(active_city_physics&&std::abs(requested-cam.y)>1.e-8){
            world_physics::Hit hit;
            if(active_city_physics->cast_capsule({cam.x,cam.y-1.75+.9,cam.z},.37f,.50f,{0,requested-cam.y,0},hit)&&hit.hit&&hit.fraction<.999f){
                const bool descending=requested<cam.y;
                requested=cam.y+(requested-cam.y)*std::max(0.f,hit.fraction-.002f);
                cam.grounded=descending&&hit.normal.y>.65;cam.vy=0;
            }
        }
        auto clear=[&](double eye) {
            return world_scenery_can_move(scene,cam.x,cam.z,cam.x,cam.z,float(eye-1.75),1.75f,.37f);
        };
        if(!clear(requested)) {
            const bool descending=requested<cam.y;
            double safe=cam.y,blocked=requested;
            if(clear(safe))for(int i=0;i<10;i++) {
                double middle=(safe+blocked)*.5;
                if(clear(middle))safe=middle;else blocked=middle;
            }
            requested=safe;
            cam.vy=0;
            cam.grounded=descending;
        }
        cam.y=requested;
        if(cam.grounded)cam.vy=0;
    }

    float millis(Uint64 begin) {
        return float(double(SDL_GetPerformanceCounter() - begin) * 1000. / SDL_GetPerformanceFrequency());
    }

    bool keyeq(WorldTileKey a, WorldTileKey b) {
        return world_tile_key_equal(a, b);
    }

    struct TerrainGPU {
        WorldTilesWorld *stream = nullptr;
        std::array<GpuTile, WORLD_TILES_CAPACITY> tiles;
        std::vector<WorldTileRequest> desired;
        std::vector<unsigned> draws;
        size_t shadow_clip_rejections=0,shadow_tile_draws=0;
        bool init() {
            WorldTilesConfig c = {};
            c.cells = 64; // 2m near sampling; 128m far silhouette spacing.
            c.height = sample_height;
            stream = world_tiles_init(&c);
            return stream != nullptr;
        }

        bool ready(const WorldTileRequest &r) const {
            return tiles[r.slot].vao && tiles[r.slot].generation == r.generation;
        }

        bool roots_ready() const {
            unsigned top = 0;
            for (const auto &q : desired)
                top = std::max(top, q.key.lod);
            size_t roots = 0;
            for (const auto &q : desired)
                if (q.key.lod == top) {
                    roots++;
                    if (!ready(q))
                        return false;
                }
            return roots == 36;
        }

        bool view_ready(double x, double z, bool fly) const {
            if (!roots_ready()) return false;
            if (fly) return true;
            for (unsigned slot : draws) {
                const auto &tile = tiles[slot];
                if (tile.key.lod == 0 && x >= tile.x && x < tile.x + tile.size &&
                    z >= tile.z && z < tile.z + tile.size) return true;
            }
            return false;
        }

        int find(WorldTileKey key) const {
            for (size_t i = 0; i < desired.size(); i++)
                if (keyeq(desired[i].key, key))
                    return (int) i;
            return -1;
        }

        void visit(int index) {
            const auto &r = desired[(size_t) index];
            if (!ready(r))
                return;
            int children[4];
            bool split = !r.leaf;
            for (int z = 0; z < 2 && split; z++)
                for (int x = 0; x < 2; x++) {
                    WorldTileKey key = {r.key.x * 2 + x, r.key.z * 2 + z, r.key.lod - 1};
                    int c = find(key);
                    children[z * 2 + x] = c;
                    if (c < 0 || !ready(desired[(size_t) c]))
                        split = false;
                }
            if (split) {
                for (int c : children)
                    visit(c);
            } else
                draws.push_back(r.slot);
        }

        void update(double x, double z, unsigned budget) {
            world_tiles_update(stream, x, z);
            WorldTileRequest r[WORLD_TILES_CAPACITY];
            size_t n = world_tiles_desired(stream, r, WORLD_TILES_CAPACITY);
            bool hierarchy_dirty=n!=desired.size();
            if(!hierarchy_dirty)for(size_t i=0;i<n;i++) {
                const auto &a=r[i],&b=desired[i];
                if(!keyeq(a.key,b.key)||a.slot!=b.slot||a.generation!=b.generation||
                   a.leaf!=b.leaf||a.generated!=b.generated) {hierarchy_dirty=true;break;}
            }
            if(hierarchy_dirty)desired.assign(r, r + n);
            for (unsigned i = 0; i < budget; i++) {
                WorldTileMesh m = {};
                if (!world_tiles_poll_completed(stream, &m))
                    break;
                if (world_tiles_is_current(stream, m.slot, m.generation)) {
                    tiles[m.slot].upload(m);
                    hierarchy_dirty=true;
                }
                world_tiles_release(&m);
            }
            // Only metadata and uploaded generations determine this hierarchy.
            // Retain the exact prior draw order while neither has changed.
            if(!hierarchy_dirty)return;
            draws.clear();
            unsigned top = 0;
            for (const auto &q : desired)
                top = std::max(top, q.key.lod);
            for (size_t i = 0; i < desired.size(); i++)
                if (desired[i].key.lod == top)
                    visit((int) i);
        }

        void draw(Program &p, const Camera &c, M4 vp, bool shadow = false,float shadow_range=260.f) {
            p.use();
            p.m4("uVP", vp);
            for (unsigned i : draws) {
                GpuTile &t = tiles[i];
                double dx = t.x + t.size * .5 - c.x, dz = t.z + t.size * .5 - c.z;
                float radius = t.size * .72f;
                if (shadow && std::hypot(dx, dz) > shadow_range + radius)
                    continue;
                if (!world_terrain_box_visible(vp.m,dx,(double(t.min_y)+t.max_y)*.5-c.y,dz,
                                               t.size*.5,(double(t.max_y)-t.min_y)*.5,t.size*.5)) {
                    if(shadow)shadow_clip_rejections++;
                    continue;
                }
                if(shadow)shadow_tile_draws++;
                p.v3("uOffset", {float(t.x - c.x), float(-c.y), float(t.z - c.z)});
                p.v2("uWorldOrigin", float(t.x), float(t.z));
                glBindVertexArray(t.vao);
                glDrawElements(GL_TRIANGLES, t.count, GL_UNSIGNED_INT, nullptr);
            }
            glBindVertexArray(0);
        }

        void release() {
            world_tiles_shutdown(stream);
            stream = nullptr;
            desired.clear();draws.clear();
            for (auto &t : tiles)
                t.release();
        }

        size_t bytes() const {
            size_t b = 0;
            for (auto &t : tiles)
                b += t.bytes;
            return b;
        }
    };

    GLuint climate_texture(GLuint &height_texture) {
        const int w = 1025, h = 577;
        std::vector<unsigned char> data((size_t) w * h * 4);
        std::vector<float> heights((size_t)w*h);
        for (int z = 0; z < h; z++)
            for (int x = 0; x < w; x++) {
                auto s = world_atlas_sample_uv(double(x) / (w - 1), double(z) / (h - 1));
                size_t at = ((size_t) z * w + x) * 4;
                heights[at/4]=s.height;
                data[at] = (unsigned char)(std::clamp(s.forest_density, 0.f, 1.f) * 255);
                data[at + 1] = (unsigned char)(std::clamp(s.moisture, 0.f, 1.f) * 255);
                data[at + 2] = (unsigned char)(s.region * 255 / 7);
                data[at + 3] = (unsigned char)(std::clamp(s.land_weight, 0.f, 1.f) * 255);
            }

        GLuint t;
        glGenTextures(1, &t);
        glBindTexture(GL_TEXTURE_2D, t);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glGenTextures(1,&height_texture);glBindTexture(GL_TEXTURE_2D,height_texture);
        glTexImage2D(GL_TEXTURE_2D,0,GL_R32F,w,h,0,GL_RED,GL_FLOAT,heights.data());
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        return t;
    }

    struct Shadow {
        GLuint fbo = 0, depth = 0;
        int side=2048;
        Program prog;
        bool init(int requested=2048) {
            int wanted=requested;
            const char *resolution=std::getenv("VALWOOD_SHADOW_RESOLUTION");
            if(resolution) {
                if(!std::strcmp(resolution,"1024"))wanted=1024;
                else if(!std::strcmp(resolution,"2048"))wanted=2048;
                else if(!std::strcmp(resolution,"3072"))wanted=3072;
                else if(!std::strcmp(resolution,"4096"))wanted=4096;
                else return false;
            }
            const char *vs = "#version 330 core\nlayout(location=0)in vec3 aPos;uniform mat4 uVP;uniform vec3 uOffset;void main(){gl_Position=uVP*vec4(aPos+uOffset,1);}";
            const char *fs = "#version 330 core\nvoid main(){}";
            prog.id = GLH_program(vs, fs);
            return prog.id&&resize(wanted);
        }
        bool resize(int wanted) {
            if(wanted!=1024&&wanted!=2048&&wanted!=3072&&wanted!=4096)return false;
            if(fbo&&side==wanted)return true;
            GLuint next_fbo=0,next_depth=0;
            glGenFramebuffers(1,&next_fbo);glBindFramebuffer(GL_FRAMEBUFFER,next_fbo);
            glGenTextures(1,&next_depth);glBindTexture(GL_TEXTURE_2D,next_depth);
            glTexImage2D(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT24,wanted,wanted,0,GL_DEPTH_COMPONENT,GL_FLOAT,nullptr);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_BORDER);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_BORDER);
            const float border[]={1,1,1,1};glTexParameterfv(GL_TEXTURE_2D,GL_TEXTURE_BORDER_COLOR,border);
            glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,next_depth,0);glDrawBuffer(GL_NONE);glReadBuffer(GL_NONE);
            const bool ok=glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE;glBindFramebuffer(GL_FRAMEBUFFER,0);
            if(!ok){glDeleteFramebuffers(1,&next_fbo);glDeleteTextures(1,&next_depth);return false;}
            glDeleteFramebuffers(1,&fbo);glDeleteTextures(1,&depth);fbo=next_fbo;depth=next_depth;side=wanted;return true;
        }

        void release() {
            prog.free();
            glDeleteFramebuffers(1, &fbo);
            glDeleteTextures(1, &depth);
        }
    };

    // Far sun cascade (AAA candidate). Terrain, tree and authored casters out
    // to `radius`, cached: re-rendered only after the camera moves 8% of the
    // radius, the sun turns ~0.3 degrees, or ten seconds pass (streaming), then
    // re-expressed relative to the current camera every frame. A refresh is
    // spread over three frames (terrain / trees+rocks / buildings) into a back
    // buffer that is swapped in when complete, so no single frame pays the
    // whole ~6 ms. The very first build happens in one frame.
    // VALWOOD_FAR_SHADOWS=0 disables; VALWOOD_FAR_SHADOW_RADIUS=300..4000.
    struct FarShadow {
        GLuint fbo[2]={0,0},depth[2]={0,0},query=0;int side=2048,front=0,phase=-1;float radius=1000.f;uint64_t refresh_frames=600;
        bool enabled=false,valid=false,query_pending=false;
        M4 cached_vp={},pending_vp={};double cx=0,cy=0,cz=0,px=0,py=0,pz=0;float sun[3]={0,0,0},pending_sun[3]={0,0,0};
        uint64_t renders=0,last_frame=0;double gpu_ms_total=0,gpu_ms_max=0;uint64_t gpu_samples=0;
        void init(){
            const char *e=std::getenv("VALWOOD_FAR_SHADOWS");if(e&&!std::strcmp(e,"0"))return;
            if(const char *q=std::getenv("VALWOOD_QA_FAR_SHADOW_REFRESH")){const int v=std::atoi(q);if(v>=1&&v<=600)refresh_frames=uint64_t(v);}
            if(const char *r=std::getenv("VALWOOD_FAR_SHADOW_RADIUS")){const float v=float(std::atof(r));if(v>=300.f&&v<=4000.f)radius=v;}
            glGenFramebuffers(2,fbo);glGenTextures(2,depth);enabled=true;
            for(int k=0;k<2;k++){
                glBindFramebuffer(GL_FRAMEBUFFER,fbo[k]);glBindTexture(GL_TEXTURE_2D,depth[k]);
                glTexImage2D(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT24,side,side,0,GL_DEPTH_COMPONENT,GL_FLOAT,nullptr);
                glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
                glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,depth[k],0);glDrawBuffer(GL_NONE);glReadBuffer(GL_NONE);
                enabled=enabled&&glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE;
            }
            glBindFramebuffer(GL_FRAMEBUFFER,0);
            glGenQueries(1,&query);
            if(!enabled)std::fprintf(stderr,"[Far shadow] framebuffer incomplete; disabled\n");
        }
        bool needs_refresh(const double cam[3],const float s[3],uint64_t frame) const {
            if(phase>=0)return false;
            if(!valid)return true;
            const double dx=cam[0]-cx,dz=cam[2]-cz,limit=radius*.08;
            if(dx*dx+dz*dz>limit*limit||std::abs(cam[1]-cy)>40.)return true;
            const float l0=std::sqrt(sun[0]*sun[0]+sun[1]*sun[1]+sun[2]*sun[2]),l1=std::sqrt(s[0]*s[0]+s[1]*s[1]+s[2]*s[2]);
            if(l0<=0||l1<=0||(sun[0]*s[0]+sun[1]*s[1]+sun[2]*s[2])/(l0*l1)<.9999863f)return true;
            return frame-last_frame>refresh_frames;
        }
        static M4 relative(M4 vp,double ox,double oy,double oz,const double cam[3]){
            M4 t=identity();t.m[12]=float(cam[0]-ox);t.m[13]=float(cam[1]-oy);t.m[14]=float(cam[2]-oz);
            return product(vp,t);
        }
        M4 current(const double cam[3]) const {return relative(cached_vp,cx,cy,cz,cam);}
        M4 pending(const double cam[3]) const {return relative(pending_vp,px,py,pz,cam);}
        void finish(uint64_t frame){
            front=1-front;cached_vp=pending_vp;cx=px;cy=py;cz=pz;std::copy_n(pending_sun,3,sun);
            valid=true;renders++;last_frame=frame;phase=-1;
        }
        GLuint texture() const {return depth[front];}
        void collect(){
            if(!query_pending)return;GLuint64 ns=0;glGetQueryObjectui64v(query,GL_QUERY_RESULT,&ns);query_pending=false;
            const double ms=double(ns)*1e-6;gpu_ms_total+=ms;gpu_ms_max=std::max(gpu_ms_max,ms);gpu_samples++;
        }
        void release(){collect();if(query)glDeleteQueries(1,&query);glDeleteFramebuffers(2,fbo);glDeleteTextures(2,depth);fbo[0]=fbo[1]=depth[0]=depth[1]=query=0;}
    };

#include "engine/world_local_light_shadow.inc"
#include "engine/world_graphics_viewer.inc"

    void teleport(Camera &camera, const WorldAtlasLandmark &p) {
        camera.x = p.x;
        camera.z = p.z;
        camera.y = std::max(world_atlas_height(p.x, p.z), 0.f) + (camera.fly ? 300. : 1.75);
        camera.vy = 0;
        camera.pitch = camera.fly ? -.35f : -.04f;
        // Give each inspection stop a useful first view of its mapped feature.
        camera.yaw = 0.f;
        if (!std::strcmp(p.id, "seldonne")) camera.yaw = 2.3f;
        else if (!std::strcmp(p.id, "ulasse")) camera.yaw = -2.1f;
        else if (!std::strcmp(p.id, "valdoa")) camera.yaw = 3.14159265f;
        else if (!std::strcmp(p.id, "brandalhall")) camera.yaw = 1.f;
        else if (!std::strcmp(p.id, "callieut")) camera.yaw = 1.5f;
        else if (!std::strcmp(p.id, "gorshett_keep")) camera.yaw = -2.2f;
        else if (!std::strcmp(p.id, "kerone_city")) camera.yaw = -1.7f;
        else if (!std::strcmp(p.id, "bellweather")) camera.yaw = -1.8f;
        else if (!std::strcmp(p.id, "estuay")) camera.yaw = 2.7f;
        else if (!std::strcmp(p.id, "fallaway")) camera.yaw = 2.9f;
        std::printf("[World] Travel to %s at %.2f %.2f height %.2f\n", p.name, p.x, p.z, world_atlas_height(p.x, p.z));
    }

    struct ScenicView {
        const char *id, *name;
        double u, v;
        float yaw, pitch;
        double eye_height;
    };
    static const ScenicView scenic_views[] = {
        {"apartment", "Opening | Apartment", .5-3498./100800., .5-11005./(100800.*2818./5009.), 3.14159265f, -.03f, 1.75},
        {"seldonne-arrival", "Seldonne | Arrival campsite", .5-11988./100800., .5+5172./(100800.*2818./5009.), 0.f, -.04f, 1.75},
        {"seldonne-road", "Seldonne | Meadow road", .5-11965./100800., .5+5300./(100800.*2818./5009.), 0.f, -.04f, 1.75},
        {"seldonne-town", "Seldonne | Town approach", .5-11968./100800., .5+5562./(100800.*2818./5009.), 0.f, -.04f, 1.75},
        {"stronghold", "Grandstone | Snowbound approach to the mountain citadel", .070, .077+265./(100800.*2818./5009.), 0.f, .12f, 1.75},
        {"stronghold-gate", "Grandstone | The great mountain gate", .070-13./100800., .077+74./(100800.*2818./5009.), .20f, .39f, 1.75},
        {"stronghold-drifts", "Grandstone | Wind-carved snow banks", .070-9./100800., .077+95./(100800.*2818./5009.), -1.15f, -.16f, 1.75},
        {"stronghold-snow", "Grandstone | Powder and packed snow", .070-17./100800., .077+114./(100800.*2818./5009.), .18f, -.65f, 1.75},
        {"stronghold-waterfall", "Grandstone | Frozen mountain cascade", .070+54./100800., .077+62./(100800.*2818./5009.), .22f, .30f, 1.75},
        {"stronghold-trees", "Grandstone | Snow-laden pines", .070-38./100800., .077+152./(100800.*2818./5009.), -.55f, .18f, 1.75},
        {"stronghold-hall", "Grandstone | Hall of the mountain clans", .070+5./100800., .077-22./(100800.*2818./5009.), -.05f, .10f, 1.75},
        {"stronghold-forge", "Grandstone | The dwarven forges", .070-16./100800., .077-29./(100800.*2818./5009.), -.94f, -.04f, 1.75},
        {"stronghold-forge-wing", "Grandstone | Clan forge arcade", .070+53./100800., .077-63./(100800.*2818./5009.), 0.f, .13f, 1.75},
        {"stronghold-throne", "Grandstone | The king beneath the mountain", .070+4./100800., .077-192./(100800.*2818./5009.), -.27f, -.04f, 1.75},
        {"stronghold-town", "Grandstone | Lower town and clan lodgings", .070, .077+188./(100800.*2818./5009.), 0.f, .12f, 1.75},
        {"stronghold-market", "Grandstone | Snowbound market arcade", .070-27./100800., .077+112./(100800.*2818./5009.), -1.0f, .14f, 1.75},
        {"stronghold-sanctuary", "Grandstone | Royal sanctuary", .070+5./100800., .077-185./(100800.*2818./5009.), -.22f, .12f, 1.75},
        {"stronghold-overlook", "Grandstone | The full mountain citadel", .070-113./100800., .077+143./(100800.*2818./5009.), .69f, .24f, 1.75},
        {"castle-village", "Valdoa | Castle village approach", .487, .320+220./(100800.*2818./5009.), 0.f, .035f, 1.75},
        {"castle-market", "Valdoa | The traders square", .487, .320+50./(100800.*2818./5009.), 0.f, .025f, 1.75},
        {"castle-stalls", "Valdoa | Market booths and traders", .487+17./100800., .320+32./(100800.*2818./5009.), .72f, -.04f, 1.75},
        {"castle-alley", "Valdoa | Merchants Lane", .487+61.75/100800., .320+47./(100800.*2818./5009.), -.0054f, .09f, 1.75},
        {"castle-training-start", "Valdoa | Master of arms training", .487-42.5/100800., .320-65.5/(100800.*2818./5009.), 0.f, -.08f, 1.75},
        {"castle-dragons", "Valdoa | Hellfire dragon landing grounds", .487+365./100800., .320+68./(100800.*2818./5009.), -1.08084f, -.06f, 1.75},
        {"castle-frost-dragon", "Valdoa | Frostshade dragon landing grounds", .487-390./100800., .320+52./(100800.*2818./5009.), 2.06075f, -.04f, 1.75},
        {"castle-dragon-flight", "Valdoa | Dragons over the western wall", .487-170./100800., .320+190./(100800.*2818./5009.), -2.37f, .66f, 1.75},
        {"castle-dragon-overlook", "Valdoa | Dragons above the castle", .487+165./100800., .320+165./(100800.*2818./5009.), -.7854f, .23f, 1.75},
        {"castle-courtyard", "Valdoa | The castle court", .487, .320-70./(100800.*2818./5009.), 0.f, .07f, 1.75},
        #include "engine/world_castle_demo_views.inc"
        #include "engine/world_wildlife_views.inc"
        #include "engine/world_bandit_views.inc"
        #include "engine/world_castle_streets_views.inc"
        #include "engine/world_bep_views.inc"
        #include "engine/world_demo_finish_views.inc"
        {"lake", "Ulasse | Silverpine shore", .4540, .1800, -.90f, .025f, 1.75},
        {"forest", "Valwood | The old pine trail", .5110, .2520, -.55f, .025f, 1.75},
        {"ridge", "Mashito | Crown of the north", .29760, .08800, .58905f, .040f, 1.75},
        {"coast", "Crosswind | The broken coast", .52200, .92525, 1.96350f, -.080f, 1.75},
        {"shore", "Ulasse | At the waterline", .4534948785, .1792875066, -.90f, -.13f, 1.75},
        {"rainforest", "Old Tanthoon | Emerald rainforest", .235-15./100800., .670+95./(100800.*2818./5009.), .20f, -.04f, 1.75},
        {"rainforest-pool", "Old Tanthoon | Rainforest reflecting pool", .235-3./100800., .670-25./(100800.*2818./5009.), -1.57079633f, -.20f, 1.75},
        {"rainforest-camp", "Old Tanthoon | Wayfarers clearing", (WORLD_TRAIL_CAMP_X-6.)/100800.+.5, (WORLD_TRAIL_CAMP_Z+4.)/(100800.*2818./5009.)+.5, .983f, -.12f, 1.75},
        {"rainforest-deep", "Old Tanthoon | Rainforest heart", .235+5./100800., .670-25./(100800.*2818./5009.), -.15f, .10f, 1.75},
        // Native authored views from scenery/starting_area_layout.json; XZ
        // positions converted through the atlas's100800m reference dimensions.
        {"start", "Silverpine | Arrival gate", .5110259269444445, .2520751675315679, -.55f, .015f, 1.75},
        {"courtyard", "Silverpine | Stone courtyard", .5108730832738095, .25149709799739417, -.0308538858f, -.10f, 1.75},
        {"workshop", "Silverpine | Working camp", .5109134050396825, .25077057077440446, .0380026035f, -.10f, 1.75},
        {"overlook", "Silverpine | Forest overlook", .5108729773611111, .2502147815019799, .3019663272f, -.06f, 1.75},
        {"grove", "Silverpine | Wildflower grove", .5101122959821428, .2505059746817124, .3772952180f, -.08f, 1.75}
    };
    bool starting_scenic(const char *id) {
        return !std::strncmp(id,"stronghold",10)||!std::strncmp(id,"castle-",7)||!std::strncmp(id,"wildlife-",9)||!std::strncmp(id,"bandit-",7)||
               !std::strncmp(id,"seldonne-",9)||
               !std::strcmp(id,"start")||!std::strcmp(id,"courtyard")||!std::strcmp(id,"workshop")||
               !std::strcmp(id,"overlook")||!std::strcmp(id,"grove")||
               !std::strcmp(id,"rainforest")||!std::strcmp(id,"rainforest-deep")||!std::strcmp(id,"rainforest-pool")||
               !std::strcmp(id,"rainforest-purchased-understory")||!std::strcmp(id,"rainforest-camp")||!std::strcmp(id,"rainforest-milestone");
    }
    void scenic_travel(Camera &cam, unsigned i) {
        const auto &view = scenic_views[i];
        world_atlas_uv_to_world(view.u,view.v,&cam.x,&cam.z);
        // The opening apartment is an elevated authored interior. Terrain
        // height is intentionally unrelated to its floor, so enter in editor
        // flight mode at the exact house spawn instead of dropping below it.
        if(!std::strcmp(view.id,"apartment")){cam.x=-3498.;cam.z=-11005.;cam.y=601.75;cam.fly=true;}
        else cam.y = std::max(world_atlas_height(cam.x,cam.z),0.f)+(cam.fly ? 120. : view.eye_height);
        cam.yaw=view.yaw; cam.pitch=!std::strcmp(view.id,"apartment")?view.pitch:(cam.fly ? -.22f : view.pitch); cam.vy=0;cam.grounded=!cam.fly;
        std::printf("[World] Scenic %s at %.3f %.3f height %.3f\n",view.id,cam.x,cam.z,cam.y);
    }

    #include "engine/world_castle_demo.inc"
    struct StartingWalkRoute {
        // Arrival, the authored closed loop, then the reverse arrival path.
        // These exact world points mirror starting_area_layout.json. Native
        // arrival captures stand5m behind point0; benchmark warmup starts here.
        static constexpr double points[][2]={
            {1108.800000,-14063.783392},{1101.269248,-14075.588064},{1093.738495,-14087.392735},
            {1085.642430,-14100.023797},{1098.396780,-14111.362516},{1105.242238,-14130.808330},
            {1107.246524,-14154.323917},{1097.645304,-14171.897095},{1073.744017,-14178.356853},
            {1050.806980,-14173.677931},{1033.482515,-14156.018290},{1025.977382,-14133.822052},
            {1028.951255,-14109.839640},{1045.372672,-14096.447960},{1066.660523,-14094.250824},
            {1085.642430,-14100.023797},{1093.738495,-14087.392735},{1101.269248,-14075.588064},
            {1108.800000,-14063.783392}
        };
        size_t next=1,steps=0,walking_frames=0,collision_blocked=0,slope_blocked=0,sliding_steps=0,waiting_frames=0;
        double attempted=0,actual=0;
        bool active=false,complete=false;
        bool rainforest=false,village=false,demo=false,hero=false,expansion=false,farms=false,bandits=false,streets=false,finished_arcade=false,trail_camp=false;
        int milestone=0;
        std::array<std::array<double,2>,11> rainforest_points={};
        std::array<std::array<double,2>,64> demo_points={};size_t demo_count=0;
        const double *point(size_t i) const {return milestone||demo||hero||expansion||farms||bandits||streets||finished_arcade||trail_camp?demo_points[i].data():(rainforest||village)?rainforest_points[i].data():points[i];}
        size_t point_count() const {return milestone||demo||hero||expansion||farms||bandits||streets||finished_arcade||trail_camp?demo_count:(rainforest||village)?rainforest_points.size():std::size(points);}
        const char *id() const {if(milestone)return milestone==6?"grandstone_frozen_basin_loop":milestone==5?"grandstone_snow_banks_loop":milestone==4?"grandstone_mountain_citadel_return":milestone==1?"watch_courtyard_return":milestone==3?"emerald_expanse_loop":"emerald_path_return";return trail_camp?"old_tanthoon_wayfarers_clearing_return":finished_arcade?"valdoa_bakers_arcade_return":streets?"valdoa_west_streets_tavern_return":bandits?"old_tanthoon_bandit_camp_loop":farms?"valdoa_castle_farms_return":expansion?"valdoa_castle_training_return":hero?"valdoa_market_hero_return":demo?"valdoa_castle_district_demo_return":village?"valdoa_castle_village_return":rainforest?"old_tanthoon_rainforest_return":"silverpine_waystation_return";}
        double length() const {
            double total=0;for(size_t i=1;i<point_count();i++)total+=std::hypot(point(i)[0]-point(i-1)[0],point(i)[1]-point(i-1)[1]);
            return total;
        }
        bool begin_milestone(Camera &cam,int which) {
            milestone=which;
            if(which==2){
                size_t count=0;const auto *path=world_atlas_rainforest_trail_points(&count);
                if(!path||count<2)return false;
                demo_count=5;demo_points[0]={path[0].x-10,path[0].z+.5032941};
                demo_points[1]={path[0].x,path[0].z};demo_points[2]={path[1].x,path[1].z};
                demo_points[3]=demo_points[1];demo_points[4]=demo_points[0];
            }else{
                constexpr double path[][2]={{-1343.4,-10289.58474745458},{-1333,-10289.58474745458},{-1333,-10279},{-1343.4,-10279},{-1343.4,-10289.58474745458}};
                demo_count=std::size(path);for(size_t i=0;i<demo_count;i++)demo_points[i]={path[i][0],path[i][1]};
            }
            begin(cam);return true;
        }
        bool begin_jungle_expansion(Camera &cam) {
            size_t count=0;const auto *path=world_atlas_rainforest_trail_points(&count);if(!path||count!=6)return false;
            milestone=3;demo_count=0;
            for(size_t i=0;i<count;i++)demo_points[demo_count++]={path[i].x,path[i].z};
            for(size_t i=1;i<std::size(kJungleLoop);i++){
                demo_points[demo_count++]={kJungleLoop[i][0],kJungleLoop[i][1]};
                if(i==4){ // Visit the dry bank of the hidden pool and return.
                    demo_points[demo_count++]={-26791.,9387.496705929329};
                    demo_points[demo_count++]={-26795.,9382.496705929329};
                    demo_points[demo_count++]={-26791.,9387.496705929329};
                    demo_points[demo_count++]={kJungleLoop[i][0],kJungleLoop[i][1]};
                }
            }
            for(size_t i=count-1;i>0;i--)demo_points[demo_count++]={path[i-1].x,path[i-1].z};
            begin(cam);return true;
        }
        bool begin_stronghold(Camera &cam) {
            milestone=4;demo_count=std::size(stronghold_route);
            if(demo_count>demo_points.size())return false;
            for(size_t i=0;i<demo_count;i++)demo_points[i]={stronghold_route[i][0],stronghold_route[i][1]};
            if(std::getenv("VALWOOD_QA_STRONGHOLD_DRIFTS")) {
                constexpr double path[][2]={{0,155},{-16,164},{-40,178},{-60,192},{-40,213},{-10,205},{0,180},{0,155}};
                milestone=5;demo_count=std::size(path);
                for(size_t i=0;i<demo_count;i++)demo_points[i]={stronghold_x+path[i][0],stronghold_z+path[i][1]};
            }
            if(std::getenv("VALWOOD_QA_STRONGHOLD_ICE")) {
                constexpr double path[][2]={{58,35},{68,35},{65,29},{58,35}};
                milestone=6;demo_count=std::size(path);
                for(size_t i=0;i<demo_count;i++)demo_points[i]={stronghold_x+path[i][0],stronghold_z+path[i][1]};
            }
            begin(cam);return true;
        }
        bool begin_rainforest(Camera &cam) {
            size_t count=0;const auto *path=world_atlas_rainforest_trail_points(&count);
            if(!path||count!=6)return false;
            rainforest=true;
            for(size_t i=0;i<rainforest_points.size();i++) {
                size_t source=i<count?i:rainforest_points.size()-1-i;
                rainforest_points[i]={path[source].x,path[source].z};
            }
            begin(cam);return true;
        }
        bool begin_village(Camera &cam) {
            constexpr double local[][2]={{0,220},{0,130},{0,55},{0,-50},{0,-88},
                {0,-50},{0,55},{-95,55},{-95,125},{0,125},{0,220}};
            double x,z;world_atlas_village_origin(&x,&z);village=true;
            for(size_t i=0;i<std::size(local);i++)rainforest_points[i]={x+local[i][0],z+local[i][1]};
            begin(cam);return true;
        }
        bool begin_demo(Camera &cam) {
            constexpr size_t n=std::size(castle_demo_route);
            if(n<2||n>demo_points.size())return false;
            double x,z;world_atlas_village_origin(&x,&z);demo=true;demo_count=n;
            for(size_t i=0;i<n;i++)demo_points[i]={x+castle_demo_route[i][0],z+castle_demo_route[i][1]};
            begin(cam);return true;
        }
        bool begin_hero(Camera &cam) {
            constexpr size_t n=std::size(castle_market_hero_route);
            if(n<2||n>demo_points.size())return false;
            double x,z;world_atlas_village_origin(&x,&z);hero=true;demo_count=n;
            for(size_t i=0;i<n;i++)demo_points[i]={x+castle_market_hero_route[i][0],z+castle_market_hero_route[i][1]};
            begin(cam);return true;
        }
        bool begin_expansion(Camera &cam) {
            constexpr size_t n=std::size(castle_expansion_route);
            if(n<2||n>demo_points.size())return false;
            double x,z;world_atlas_village_origin(&x,&z);expansion=true;demo_count=n;
            for(size_t i=0;i<n;i++)demo_points[i]={x+castle_expansion_route[i][0],z+castle_expansion_route[i][1]};
            begin(cam);return true;
        }
        bool begin_farms(Camera &cam) {
            constexpr size_t n=std::size(castle_farms_route);
            if(n<2||n>demo_points.size())return false;
            double x,z;world_atlas_village_origin(&x,&z);farms=true;demo_count=n;
            for(size_t i=0;i<n;i++)demo_points[i]={x+castle_farms_route[i][0],z+castle_farms_route[i][1]};
            begin(cam);return true;
        }
        bool begin_bandits(Camera &cam) {
            constexpr size_t n=std::size(bandit_walk_route);
            if(n<2||n>demo_points.size())return false;
            bandits=true;demo_count=n;
            for(size_t i=0;i<n;i++)demo_points[i]={bandit_walk_route[i][0],bandit_walk_route[i][1]};
            begin(cam);return true;
        }
        bool begin_streets(Camera &cam) {
            constexpr size_t n=std::size(castle_streets_route);
            if(n<2||n>demo_points.size())return false;
            streets=true;demo_count=n;double x,z;world_atlas_village_origin(&x,&z);
            for(size_t i=0;i<n;i++)demo_points[i]={x+castle_streets_route[i][0],z+castle_streets_route[i][1]};
            begin(cam);return true;
        }
        bool begin_trail_camp(Camera &cam) {
            constexpr size_t n=std::size(world_trail_camp_approach);
            static_assert(n*2-1<=32,"Bounded camp walk");
            trail_camp=true;demo_count=n*2-1;
            for(size_t i=0;i<n;i++)demo_points[i]={world_trail_camp_approach[i][0],world_trail_camp_approach[i][1]};
            for(size_t i=n;i<demo_count;i++)demo_points[i]=demo_points[demo_count-1-i];
            begin(cam);return true;
        }
        bool begin_finished_arcade(Camera &cam) {
            // Consume the exact generated paving segments also used by the
            // production atlas. No duplicate waypoint table or authoring recook.
            struct VillageRoad {double ax,az,bx,bz,width,paving;};
            struct VillageRect {double x,z,hx,hz,c,s,paving,grass;};
            #include "engine/world_demo_finish_ground.inc"
            (void)demo_finish_pads;
            constexpr size_t segments=std::size(demo_finish_roads),count=segments*2+1;
            if(segments!=8||count>demo_points.size())return false;
            double x,z;world_atlas_village_origin(&x,&z);
            for(size_t i=0;i<segments;i++) {
                const auto &r=demo_finish_roads[i];
                if(!std::isfinite(r.ax)||!std::isfinite(r.az)||!std::isfinite(r.bx)||!std::isfinite(r.bz)||
                   r.width!=1.3||r.paving!=.8||std::hypot(r.bx-r.ax,r.bz-r.az)<=0)return false;
                if(i&&(r.ax!=demo_finish_roads[i-1].bx||r.az!=demo_finish_roads[i-1].bz))return false;
                if(!i)demo_points[0]={x+r.ax,z+r.az};
                demo_points[i+1]={x+r.bx,z+r.bz};
            }
            for(size_t i=segments+1;i<count;i++)demo_points[i]=demo_points[count-1-i];
            finished_arcade=true;demo_count=count;begin(cam);return true;
        }
        void begin(Camera &cam) {
            active=true;cam.fly=false;cam.grounded=true;cam.x=point(0)[0];cam.z=point(0)[1];
            cam.y=walking_height(cam.x,cam.z,nullptr)+1.75;cam.vy=0;cam.pitch=.015f;
            cam.yaw=float(std::atan2(point(1)[0]-cam.x,-(point(1)[1]-cam.z)));
        }
        void advance(Camera &cam,const WorldScenery *scene) {
            if(complete)return;
            const size_t previous_waypoint=next;
            walking_frames++;
            double remaining=world_atlas_walk_speed()/60.;
            while(remaining>1.e-8&&next<point_count()) {
                double dx=point(next)[0]-cam.x,dz=point(next)[1]-cam.z,distance=std::hypot(dx,dz);
                if(distance<1.e-6) {next++;continue;}
                double step=std::min(remaining,distance);
                auto moved=move_camera(cam,scene,cam.x+dx/distance*step,cam.z+dz/distance*step);
                attempted+=moved.attempted;actual+=moved.actual;steps++;
                collision_blocked+=moved.collision;slope_blocked+=moved.slope;sliding_steps+=moved.sliding;
                cam.y=walking_height(cam.x,cam.z,nullptr)+1.75;cam.vy=0;cam.grounded=true;
                const float yaw=float(std::atan2(dx,-dz));
                cam.yaw+=std::remainder(yaw-cam.yaw,6.2831853f)*.10f;
                remaining-=step;
                if(std::hypot(point(next)[0]-cam.x,point(next)[1]-cam.z)<1.e-6)next++;
                if(moved.actual<step*.999)break;
            }
            complete=next==point_count();
            if(milestone==3&&next!=previous_waypoint){std::printf("[Jungle route] Reached %zu/%zu at %.2f %.2f; walked %.2fm; collision %zu slope %zu\n",next-1,point_count()-1,cam.x,cam.z,actual,collision_blocked,slope_blocked);std::fflush(stdout);}
            if(milestone==4&&(next!=previous_waypoint||walking_frames%1800==0)){std::printf("[Grandstone route] Reached %zu/%zu at %.2f %.2f; walked %.2fm; collision %zu slope %zu\n",next-1,point_count()-1,cam.x,cam.z,actual,collision_blocked,slope_blocked);std::fflush(stdout);}
        }
    };

    bool world_live_visuals_validate(const world_studio::Document&,const world_runtime::Document&,const world_runtime_demo::Seed&,const std::vector<world_runtime::Entity>&,std::string&);
    #include "engine/world_structure_render.inc"
    #include "engine/world_studio_view.inc"
    #include "engine/world_asset_browser_qa.inc"
    #include "engine/world_editor_long_session_qa.inc"
    #include "engine/world_editor_structure_spline_qa.inc"
    #include "engine/world_runtime_visuals.inc"

    struct Lighting {
        V3 sun, sunColor, ambient, fog, skyLight, groundLight, skyZenith;
        float haze, cloud, time;
        float environment_gain=1.f;
    };

    Lighting lighting(float hour, float cloud, float time=0, float bearing=0,
                      float sun_strength=1.f, float sky_fill=1.f) {
        const float angle=(hour-6.f)/12.f*3.14159265f;
        const float elevation=std::max(std::sin(angle),-.2f);
        cloud=std::clamp(cloud,0.f,1.f);
        const float daylight=std::clamp((elevation+.06f)/.24f,0.f,1.f);
        // North is -Z on the atlas. A southern solar arc gives the settlement's
        // south-facing stone/timber facades raking light rather than permanent
        // backlight. Rotate the same source for every surface, cloud and shadow.
        const float azimuth=bearing*(3.14159265f/180.f);
        const float arc_x=std::cos(angle)*.65f,arc_z=.35f;
        const V3 sun=norm({arc_x*std::cos(azimuth)-arc_z*std::sin(azimuth),
                           elevation,arc_x*std::sin(azimuth)+arc_z*std::cos(azimuth)});
        const float warmth=std::pow(1.f-std::clamp(sun.y/.65f,0.f,1.f),1.35f);
        const float direct=1.62f*daylight*(1.f-.72f*cloud)*sun_strength;
        // Diffuse irradiance/PI is distinct from the visible sky radiance.
        // A cooler open sky and restrained warm ground make sun/shade legible.
        const V3 skyLight=mul({.010f+daylight*(.19f+.065f*cloud),
                           .014f+daylight*(.245f+.030f*cloud),
                           .023f+daylight*(.325f-.015f*cloud)},sky_fill);
        if(city_showcase){
            auto rgb=[](const std::array<float,3> &v,float scale=1.f){return V3{v[0]*scale,v[1]*scale,v[2]*scale};};
            const V3 sky=rgb(city_look.sky,sky_fill);
            return {sun,rgb(city_look.sun,direct),sky,rgb(city_look.fog),sky,
                rgb(city_look.ground,sky_fill),rgb(city_look.zenith),city_look.haze,cloud,time};
        }
        return {sun,{direct,direct*(.97f-.25f*warmth),direct*(.88f-.38f*warmth)},
            skyLight,{.018f+daylight*.345f,.025f+daylight*.405f,.040f+daylight*.465f},
            skyLight,mul({.004f+daylight*.066f,.005f+daylight*.070f,.006f+daylight*.047f},sky_fill),
            {.004f+daylight*(.040f+.065f*cloud),.009f+daylight*(.116f+.045f*cloud),
             .022f+daylight*(.245f-.015f*cloud)},.000025f+cloud*.000030f,cloud,time};
    }

    Lighting local_lighting(float hour,float cloud,float time,float rainforest,
                            float bearing=0,float sun_strength=1.f,float sky_fill=1.f) {
        if(city_showcase)return lighting(hour,cloud,time,bearing,sun_strength,sky_fill);
        const auto atmosphere=world_rainforest_atmosphere(cloud,rainforest,visual_surface_finish);
        if(atmosphere.blend<=0)return lighting(hour,cloud,time,bearing,sun_strength,sky_fill);
        Lighting light=lighting(hour,atmosphere.cloud,time,bearing,
            sun_strength*atmosphere.sun_multiplier,sky_fill*atmosphere.sky_multiplier);
        light.environment_gain=atmosphere.sky_multiplier;
        // Soft overcast illumination beneath the canopy. Distant aerial haze
        // remains available on low settings; the existing8-step volume supplies
        // the separate, terrain-following humid ground layer when enabled.
        light.haze+=atmosphere.haze_add;
        const float daylight=std::clamp((light.fog.x-.018f)/.345f,0.f,1.f);
        const V3 humid={.006f+.180f*daylight,.009f+.215f*daylight,.010f+.205f*daylight};
        const V3 overcast={.003f+.068f*daylight,.005f+.086f*daylight,.008f+.090f*daylight};
        light.fog=add(mul(light.fog,1-atmosphere.blend),mul(humid,atmosphere.blend));
        const float overcast_blend=atmosphere.blend*(visual_surface_finish?.45f:1.f);
        light.skyZenith=add(mul(light.skyZenith,1-overcast_blend),mul(overcast,overcast_blend));
        // A bounded diffuse vegetation/soil return keeps nearby trunks and
        // ground readable beneath the overcast canopy. Fog and sun stay fixed.
        light.groundLight=add(light.groundLight,mul(light.skyLight,.12f*atmosphere.blend));
        return light;
    }

    void rainforest_materials(Program &land) {
        double x,z;world_atlas_uv_to_world(.235,.670,&x,&z);
        land.use();land.f("uRainforest",1.f);
        const auto *pool=world_atlas_rainforest_pool();
        glUniform4f(land.at("uFreshwaterPool"),float(pool->x),float(pool->z),float(1./pool->radius_x),float(1./pool->radius_z));
        land.f("uFreshwaterLevel",float(pool->surface_y));
        glUniform4f(land.at("uRainforestBounds"),float(x),float(z),float(1./(world_atlas_width()*.065)),float(1./(world_atlas_depth()*.085)));
        size_t count=0;const auto *path=world_atlas_rainforest_trail_points(&count);
        for(size_t i=0;path&&i<count;i++) {
            char name[48];std::snprintf(name,sizeof(name),"uRainforestPath[%zu]",i);
            land.v2(name,float(path[i].x),float(path[i].z));
        }
    }

    float foliage_detail_scale=1.f;
    unsigned environment_texture_edge=2048; //Fixed at startup; excludes actors, weapons and UI.
    float foliage_density_scale=1.f;
    WorldFoliageDensity foliage_budget_tier=WorldFoliageDensity::Ultra;
    void light_program(Program &p, const Lighting &l,const Camera &camera) {
        studio_decals(p,camera);
        // Reset at every pass boundary. Reflection/probe/loading/actor owners
        // keep complete source coverage; only the primary foliage owner opts in.
        p.v2("uCameraFoliageFade",0.f,0.f);
        p.f("uFoliageDistanceScale",foliage_detail_scale);
        p.v2("uTreeNearFade",WORLD_TREE_NEAR_FADE_BEGIN*foliage_detail_scale,WORLD_TREE_NEAR_FADE_END*foliage_detail_scale);
        if(studio_environment&&studio_document){const auto &e=studio_document->environment;world_environment_bind(studio_environment,p.id,e.rotation,e.intensity*l.environment_gain,e.enabled?e.strength:0.f);}
        p.i("uSoftShadows",1);
        p.v3("uSun",l.sun);p.v3("uSunColor",l.sunColor);
        p.v3("uAmbient",l.ambient);p.v3("uFog",l.fog);
        p.v3("uSkyLight",l.skyLight);p.v3("uGroundLight",l.groundLight);
        p.v3("uSkyZenith",l.skyZenith);p.f("uHaze",l.haze);
        p.f("uCloud",l.cloud);p.f("uTime",l.time);
        const auto &flash=active_storm_light;
        glUniform4f(p.at("uStormFlashPositionRadius"),float(flash.x-camera.x),float(flash.y-camera.y),float(flash.z-camera.z),flash.active?flash.radius:0.f);
        glUniform3f(p.at("uStormFlashColor"),flash.color[0]*flash.gain,flash.color[1]*flash.gain,flash.color[2]*flash.gain);
        glUniform4fv(p.at("uHomeRoomLights"),1,world_opening_room_lights::emission_weights.data());
        glUniform1f(p.at("uHomeNight"),world_apartment_window_light::night);
        glUniform4fv(p.at("uHomeGameState"),1,world_opening_game_screen::state.data());
    }

    void torch_program(Program &p,const WorldTorchLights &lights) {
        local_light_shadow.bind(p,local_light_shadows_supported);
        if(!p.torch_cached) {
            const char *names[]={"uTorchCount","uTorchPositionRadius[0]","uTorchColorIntensity[0]",
                "uTorchWallPlane[0]","uTorchClipBounds[0]","uTorchClipHeights[0]","uTorchClusterMin","uTorchClusterMax","uTorchDirectionCone[0]","uActorTorchOnly"};
            for(unsigned i=0;i<p.torch_uniforms.size();i++)p.torch_uniforms[i]=glGetUniformLocation(p.id,names[i]);
            p.torch_cached=true;
        }
        const auto &u=p.torch_uniforms;
        // Source practicals illuminate explicit actor draws only. Programs
        // without that gate (terrain/water/ordinary projects) receive none.
        glUniform1i(u[9],lights.actor_only?1:0);
        glUniform1i(u[0],lights.actor_only&&u[9]<0?0:int(lights.count));
        if(!lights.count)return;
        glUniform4fv(u[1],lights.count,lights.position_radius[0].data());
        glUniform4fv(u[2],lights.count,lights.color_intensity[0].data());
        glUniform4fv(u[3],lights.count,lights.plane[0].data());
        glUniform4fv(u[4],lights.count,lights.bounds[0].data());
        glUniform2fv(u[5],lights.count,lights.heights[0].data());
        glUniform3fv(u[6],1,lights.cluster_min.data());
        glUniform3fv(u[7],1,lights.cluster_max.data());
        glUniform4fv(u[8],lights.count,lights.direction_cone[0].data());
    }
    WorldTorchFlameFrame torch_frame(const Camera &cam,const Lighting &light,V3 right,V3 up,V3 forward,
                                    const M4 &vp,unsigned fbo,int width,int height,bool clip=false,float level=0) {
        WorldTorchFlameFrame f;
        f.framebuffer=fbo;f.width=width;f.height=height;
        f.camera[0]=cam.x;f.camera[1]=cam.y;f.camera[2]=cam.z;
        std::memcpy(f.vp,vp.m,sizeof(vp.m));
        auto copy=[](float *out,V3 v){out[0]=v.x;out[1]=v.y;out[2]=v.z;};
        copy(f.right,right);copy(f.up,up);copy(f.forward,forward);copy(f.fog,light.fog);
        f.wind_xz[0]=.16f;f.wind_xz[1]=.05f;
        f.time=light.time;f.haze=light.haze;f.clip_below=clip;f.clip_height=level;return f;
    }

    #include "world_corner_minimap.inc"

    void weather_program(Program &p,float wx,float wz,float wet) {
        WorldWeatherAuthoredUniforms authored_locations;
        authored_locations.authored_count=p.at("uAuthoredWeatherCount");authored_locations.area_count=p.at("uAuthoredWeatherAreaCount");
        authored_locations.global=p.at("uAuthoredWeatherGlobal");authored_locations.bounds=p.at("uAuthoredWeatherBounds[0]");authored_locations.channels=p.at("uAuthoredWeatherChannels[0]");
        world_weather_bind_authored(authored_locations,active_authored_weather);
        const WorldWeatherWetnessUniforms wet_locations{p.at("uWetStateEnabled"),p.at("uWetStateCount"),p.at("uWetStateGlobal"),p.at("uWetStateBounds[0]"),p.at("uWetStateChannels[0]")};
        world_weather_bind_wetness(wet_locations,active_weather_wetness);
        p.v2("uWeatherWind",wx,wz);p.f("uWetness",stronghold_finish?0.f:wet);studio_surface_gpu.bind_wind(p);p.i("uVegetationFinish",visual_surface_finish?1:0);glUniform4fv(p.at("uGrassInteractor"),1,visual_grass_interactor.data());
        glUniform4fv(p.at("uGrassInteractionHistory[0]"),3,grass_interaction.history[0].data());
        glUniform4fv(p.at("uGrassInteractionBounds"),1,grass_interaction.bounds.data());
    }
    float weather_surface(double x,double z,void*) {
        float height=world_atlas_height(x,z),level=0;
        return world_atlas_local_water(x,z,&level,nullptr)?std::max(height,level):height;
    }
    WorldWeatherFrame weather_frame(const Camera &cam,const Lighting &light,V3 right,V3 up,V3 forward,
                                   const M4 &light_vp,int width,int height,float jx,float jy,float rain,float fog,float wx,float wz) {
        WorldWeatherFrame f;f.near_plane=view_near_plane();f.tan_half_fov=std::tan(cam.fov*.5f);
        auto copy=[](float *out,V3 v){out[0]=v.x;out[1]=v.y;out[2]=v.z;};
        f.width=width;f.height=height;f.jitter_ndc[0]=jx;f.jitter_ndc[1]=jy;
        copy(f.right,right);copy(f.up,up);copy(f.forward,forward);copy(f.sun,light.sun);copy(f.sun_color,light.sunColor);
        copy(f.fog,light.fog);copy(f.sky_light,light.skyLight);copy(f.ground_light,light.groundLight);
        f.camera[0]=cam.x;f.camera[1]=cam.y;f.camera[2]=cam.z;
        std::memcpy(f.light_vp,light_vp.m,sizeof(light_vp.m));
        f.world_size[0]=float(world_atlas_width());f.world_size[1]=float(world_atlas_depth());
        f.time=light.time;f.intensity=rain;f.fog_strength=fog;f.wind_xz[0]=wx;f.wind_xz[1]=wz;
        f.authored=active_authored_weather;f.wetness=active_weather_wetness;
        const auto *pool=world_atlas_rainforest_pool();
        f.pool_bounds[0]=float(pool->x);f.pool_bounds[1]=float(pool->z);
        f.pool_bounds[2]=float(pool->radius_x);f.pool_bounds[3]=float(pool->radius_z);f.pool_level=float(pool->surface_y);
        f.surface_height=weather_surface;return f;
    }

    WorldSceneParticlesFrame scene_particle_frame(const Camera &cam,const Lighting &light,V3 right,V3 up,V3 forward,
                                                   const M4 &vp,const M4 &light_vp,int width,int height,
                                                   unsigned color,unsigned depth,unsigned shadow,const WorldEnvironmentEffectsProfile &effects,float hour,float wx,float wz) {
        WorldSceneParticlesFrame f;f.near_plane=view_near_plane();f.tan_half_fov=std::tan(cam.fov*.5f);
        f.color=color;f.depth=depth;f.shadow=shadow;f.width=width;f.height=height;
        f.camera[0]=cam.x;f.camera[1]=cam.y;f.camera[2]=cam.z;f.time=light.time;
        std::memcpy(f.vp,vp.m,sizeof(vp.m));std::memcpy(f.light_vp,light_vp.m,sizeof(light_vp.m));
        auto copy=[](float *out,V3 v){out[0]=v.x;out[1]=v.y;out[2]=v.z;};
        copy(f.right,right);copy(f.up,up);copy(f.forward,forward);copy(f.sun,light.sun);copy(f.sun_color,light.sunColor);
        copy(f.sky_light,light.skyLight);copy(f.ground_light,light.groundLight);copy(f.fog,light.fog);
        f.smoke_budget=effects.smoke_particles;f.ambient_budget=effects.ambient_particles;f.smoke_distance=effects.smoke_distance;f.life_distance=effects.life_distance;f.hour=hour;
        f.haze=light.haze;f.wind_xz[0]=wx;f.wind_xz[1]=wz;if(!studio_surface_gpu.data.winds.empty()){f.local_wind=WorldSurfaceGPU::particle_wind;f.wind_userdata=&studio_surface_gpu.data;}return f;
    }

    WorldSkyFrame sky_frame(const Camera &cam,const Lighting &light,V3 right,V3 up,V3 forward,float jx,float jy,bool source_background=true) {
        WorldSkyFrame frame;
        auto copy=[](float *out,V3 v){out[0]=v.x;out[1]=v.y;out[2]=v.z;};
        copy(frame.right,right);copy(frame.up,up);copy(frame.forward,forward);
        copy(frame.eye,{float(cam.x),float(cam.y),float(cam.z)});copy(frame.sun,light.sun);copy(frame.sun_color,light.sunColor);
        copy(frame.fog,light.fog);copy(frame.ambient,light.ambient);copy(frame.sky_light,light.skyLight);
        copy(frame.ground_light,light.groundLight);copy(frame.sky_zenith,light.skyZenith);
        frame.aspect=cam.aspect;frame.tan_half_fov=std::tan(cam.fov*.5f);frame.cloud=light.cloud;frame.time=light.time;
        frame.authored=active_authored_weather;frame.lightning=active_storm_light;
        frame.jitter_ndc[0]=jx;frame.jitter_ndc[1]=jy;if(studio_environment&&studio_document){auto stats=world_environment_stats(studio_environment);const auto &e=studio_document->environment;frame.environment_texture=stats.raw;frame.environment_rotation=e.rotation;frame.environment_intensity=e.intensity*light.environment_gain;frame.environment_strength=e.enabled?e.strength:0.f;}
        if(source_background&&city_showcase&&active_city_sky&&active_city_sky->ready()&&!active_authored_weather.global_authored&&!active_authored_weather.authored_count){
            frame.environment_texture=active_city_sky->texture;frame.environment_rotation=0;frame.environment_intensity=1;frame.environment_strength=1;
            frame.cloud=0;frame.sun_disc=false;frame.aerial_horizon=1;
        }
        return frame;
    }

    bool castle_creek_nearby(const Camera &cam) {
        // Purchased city/countryside scenes own their water. The legacy
        // fixed-Y castle moat does not belong on authored source terrain.
        if(city_showcase)return false;
        double ox,oz;world_atlas_village_origin(&ox,&oz);
        const double x=cam.x-ox,z=cam.z-oz;
        if(world_castle_hall_enclosed_eye(x,cam.y,z))return false;
        if(cam.y>1000||x<WORLD_CASTLE_CREEK_MIN_X-120||x>WORLD_CASTLE_CREEK_MAX_X+120||
           z<WORLD_CASTLE_CREEK_MIN_Z-120||z>WORLD_CASTLE_CREEK_MAX_Z+120)return false;
        for(size_t i=1;i<std::size(world_castle_creek_points);i++) {
            const auto &a=world_castle_creek_points[i-1],&b=world_castle_creek_points[i];
            const double dx=b.x-a.x,dz=b.z-a.z;
            const double t=std::clamp(((x-a.x)*dx+(z-a.z)*dz)/(dx*dx+dz*dz),0.,1.);
            // Match the visible capsule's float-rounded radius exactly.
            const float radius=float(std::max(a.half_width,b.half_width)+4.);
            if(std::hypot(x-a.x-dx*t,z-a.z-dz*t)<=120.+radius)return true;
        }
        return false;
    }

    bool castle_creek_visible(const Camera &cam,V3 forward,V3 right,V3 up) {
        // Purchased city/countryside scenes own their water. The legacy
        // fixed-Y castle moat does not belong on authored source terrain.
        if(city_showcase)return false;
        double ox,oz;world_atlas_village_origin(&ox,&oz);
        const double x=cam.x-ox,z=cam.z-oz;
        if(world_castle_hall_enclosed_eye(x,cam.y,z))return false;
        if(cam.y>1000||x<WORLD_CASTLE_CREEK_MIN_X-120||x>WORLD_CASTLE_CREEK_MAX_X+120||
           z<WORLD_CASTLE_CREEK_MIN_Z-120||z>WORLD_CASTLE_CREEK_MAX_Z+120)return false;
        const float tv=std::tan(cam.fov*.5f),th=tv*cam.aspect;
        for(size_t i=1;i<std::size(world_castle_creek_points);i++) {
            const auto &a=world_castle_creek_points[i-1],&b=world_castle_creek_points[i];
            const double dx=b.x-a.x,dz=b.z-a.z;
            const double t=std::clamp(((x-a.x)*dx+(z-a.z)*dz)/(dx*dx+dz*dz),0.,1.);
            const float radius=float(std::max(a.half_width,b.half_width)+4.);
            if(std::hypot(x-a.x-dx*t,z-a.z-dz*t)>120.+radius)continue;
            V3 da={float(a.x-x),float(WORLD_CASTLE_CREEK_SURFACE_Y-cam.y),float(a.z-z)};
            V3 db={float(b.x-x),da.y,float(b.z-z)};
            const float za=dot(da,forward),zb=dot(db,forward);
            if(std::max(za,zb)<-radius)continue;
            bool outside=false;
            // Each plane bounds the entire variable-width segment capsule,
            // including long straight moat runs; no gaps between sample spheres.
            for(int sign:{-1,1}) {
                outside|=std::min(sign*dot(da,right)-za*th,sign*dot(db,right)-zb*th)>radius*std::sqrt(1+th*th);
                outside|=std::min(sign*dot(da,up)-za*tv,sign*dot(db,up)-zb*tv)>radius*std::sqrt(1+tv*tv);
            }
            if(!outside)return true;
        }
        return false;
    }

    struct LandscapeMaterials {
        GLuint masks=0,starting=0,village_pbr=0,city_coverage=0;
        std::array<float,4> city_ground_bounds={},countryside_ground_bounds={};
        bool countryside_ground_ready=false;
        WorldVillageTextures detail;
        WorldCastleLights castle_lights;
        bool castle_lights_ready=false;
        WorldTavernLighting tavern_lighting;
        WorldForestShelter forest_shelter;
        // Optional publisher roof raster is weather cover only; atlas terrain
        // and walk/collision heights retain their independent source field.
        WorldHeightRegion source_roofs={};
        bool forest_shelter_enabled=std::getenv("VALWOOD_QA_DISABLE_FOREST_SHELTER")==nullptr;
        bool tavern_lighting_ready=false;
        bool tavern_return_enabled=std::getenv("VALWOOD_QA_DISABLE_TAVERN_RETURN")==nullptr;
        bool directional_enabled=std::getenv("VALWOOD_DISABLE_VILLAGE_DIRECTIONAL")==nullptr;
        bool history_enabled=std::getenv("VALWOOD_DISABLE_VILLAGE_GROUND_HISTORY")==nullptr;
        std::array<float,12> bounds={};
        std::array<float,27> trail={};
        std::array<float,4> trail_bounds={};
        std::array<float,4> starting_bounds={},village_bounds={},camp_bounds={};
        std::array<float,4> ground_history_bounds={};
        // Retain the existing startup roof bake for the bounded weather cache.
        // Conservative caps stop drops and suppress ground splashes under roofs;
        // caps are never used as sloping splash geometry or fog terrain.
        std::vector<float> weather_roofs;
        std::array<double,4> weather_roof_bounds={};
        static bool rain_contact(double x,double z,WorldRainContact &out,void*) {
            float level=0,depth=0;
            if(world_atlas_local_water(x,z,&level,&depth)&&level>world_atlas_height(x,z)+.025f){out.material=WorldRainMaterial::Water;return true;}
            const auto start=world_atlas_starting_sample(x,z),village=world_atlas_village_sample(x,z);
            const auto ground=world_atlas_landscape_sample(x,z);
            // These are the same cached paving/rock weights used by terrain
            // materials, rather than a town-sized rectangle guessed as stone.
            if(std::max(start.paving,village.paving)>.5||ground.rock>.65f)out.material=WorldRainMaterial::Stone;
            else if(world_atlas_sample(x,z).moisture>.15f||ground.trail>.25f)out.material=WorldRainMaterial::Mud;
            return true;
        }
        static float cover_height(double x,double z,void *userdata) {
            if(stronghold_finish&&!source_showcase){const float roof=world_nature_grandstone_roof(x-stronghold_x,z-stronghold_z);if(roof>0)return world_atlas_height(x,z)+roof;}
            if(stronghold_finish&&!source_showcase&&std::abs(x-stronghold_x)<9&&z>=stronghold_z+12&&z<stronghold_z+30.4) {
                // Follow the new pointed stone throat so snow stops at the
                // arch overhead instead of falling through its underside.
                const double side=std::abs(x-stronghold_x);
                const double roof=side<=5.2?25.8-side*3.4/5.2:22.4-(side-5.2)*5.6/3.8;
                return world_atlas_height(x,z)+float(roof);
            }
            const auto &a=*static_cast<const LandscapeMaterials*>(userdata);
            float source_roof=0;
            if(world_height_region_sample(&a.source_roofs,x,z,&source_roof)&&source_roof>0)return source_roof;
            if(a.weather_roofs.empty())return 0;
            const auto &b=a.weather_roof_bounds;
            const double u=(x-b[0])/b[2]*512.,v=(z-b[1])/b[3]*512.;
            if(u<0||v<0||u>512||v>512)return 0;
            const int ix=std::min(511,int(u)),iz=std::min(511,int(v));
            float cap=0;
            for(int dz=0;dz<2;dz++)for(int dx=0;dx<2;dx++)
                cap=std::max(cap,a.weather_roofs[size_t(iz+dz)*513+ix+dx]);
            return cap;
        }
        // One array occupies the terrain pass's spare unit10. Albedo mips are
        // averaged in linear light, normal/ARM mips stay linear. Decode only
        // albedo in GLSL: applying sRGB sampling to all three layers corrupts
        // the packed roughness and normal data.
        bool load_village_pbr() {
            constexpr int side=2048;
            const char *files[]={"ground/village_stone_BC.png","ground/village_stone_N.png","ground/village_stone_ARM.png"};
            std::vector<unsigned char> level(size_t(side)*side*4*3);
            for(int layer=0;layer<3;layer++) {
                int w=0,h=0,c=0;auto *data=stbi_load(files[layer],&w,&h,&c,4);
                if(!data||w!=side||h!=side) {
                    std::fprintf(stderr,"[World] Invalid village PBR layer %s\n",files[layer]);
                    stbi_image_free(data);return false;
                }
                std::copy_n(data,size_t(side)*side*4,level.data()+size_t(layer)*side*side*4);
                stbi_image_free(data);
            }
            // Unreal source normals use DirectX green; our tangent frame is GL.
            for(size_t p=size_t(side)*side*4+1;p<size_t(side)*side*4*2;p+=4)level[p]=255-level[p];
            float linear[256];for(int i=0;i<256;i++) {
                float s=i/255.f;linear[i]=s<=.04045f?s/12.92f:std::pow((s+.055f)/1.055f,2.4f);
            }
            glGenTextures(1,&village_pbr);glBindTexture(GL_TEXTURE_2D_ARRAY,village_pbr);
            int w=side,mip=0;
            while(true) {
                glTexImage3D(GL_TEXTURE_2D_ARRAY,mip,GL_RGBA8,w,w,3,0,GL_RGBA,GL_UNSIGNED_BYTE,level.data());
                if(w==1)break;
                const int next=w/2;std::vector<unsigned char> down(size_t(next)*next*4*3);
                for(int layer=0;layer<3;layer++)for(int y=0;y<next;y++)for(int x=0;x<next;x++)for(int c=0;c<4;c++) {
                    float mean=0;
                    for(int dy=0;dy<2;dy++)for(int dx=0;dx<2;dx++) {
                        auto v=level[((size_t(layer)*w+y*2+dy)*w+x*2+dx)*4+c];
                        mean+=(layer==0&&c<3)?linear[v]:v/255.f;
                    }
                    mean*=.25f;
                    if(layer==0&&c<3)mean=mean<=.0031308f?12.92f*mean:1.055f*std::pow(mean,1.f/2.4f)-.055f;
                    down[((size_t(layer)*next+y)*next+x)*4+c]=(unsigned char)std::lround(std::clamp(mean,0.f,1.f)*255);
                }
                level.swap(down);w=next;mip++;
            }
            glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_WRAP_S,GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_WRAP_T,GL_REPEAT);
            if(SDL_GL_ExtensionSupported("GL_EXT_texture_filter_anisotropic")||SDL_GL_ExtensionSupported("GL_ARB_texture_filter_anisotropic")) {
                GLfloat maximum=1;glGetFloatv(0x84FF,&maximum);glTexParameterf(GL_TEXTURE_2D_ARRAY,0x84FE,std::min(maximum,4.f));
            }
            return glGetError()==GL_NO_ERROR;
        }
        bool init(const WorldScenery *scenery,bool source_pbr=false) {
            if(city_showcase){
                WorldHeightRegion coverage={};char error[256];
                const int loaded=world_height_region_load(&coverage,"ground/city.coverage",error,sizeof(error));
                if(loaded!=1){std::fprintf(stderr,"[City] Ground coverage rejected: %s\n",loaded<0?error:"missing source ground mask");return false;}
                WorldHeightRegion countryside={};
                const int surrounding=world_height_region_load(&countryside,"ground/countryside.coverage",error,sizeof(error));
                if(surrounding<0){std::fprintf(stderr,"[City] Countryside coverage rejected: %s\n",error);world_height_region_clear(&coverage);return false;}
                if(surrounding>0&&(countryside.columns!=coverage.columns||countryside.rows!=coverage.rows||!world_atlas_fallback_height_region_info(nullptr))){
                    std::fprintf(stderr,"[City] Countryside coverage requires matching source mask dimensions and countryside.height\n");world_height_region_clear(&coverage);world_height_region_clear(&countryside);return false;
                }
                const size_t nodes=size_t(coverage.columns)*coverage.rows;
                std::vector<unsigned char> pixels(nodes*(surrounding>0?2:1));
                for(size_t i=0;i<nodes;i++)pixels[i]=coverage.heights[i]>.5f?255:0;
                if(surrounding>0){
                    for(size_t i=0;i<nodes;i++){
                        if(countryside.heights[i]<0||countryside.heights[i]>1){std::fprintf(stderr,"[City] Countryside mask samples must be in0..1\n");world_height_region_clear(&coverage);world_height_region_clear(&countryside);return false;}
                        pixels[nodes+i]=countryside.heights[i]>.5f?255:0;
                    }
                    countryside_ground_bounds={float(countryside.xmin),float(countryside.zmin),float(1./((countryside.columns-1)*countryside.stepx)),float(1./((countryside.rows-1)*countryside.stepz))};
                    countryside_ground_ready=true;
                }
                city_ground_bounds={float(coverage.xmin),float(coverage.zmin),
                    float(1./((coverage.columns-1)*coverage.stepx)),float(1./((coverage.rows-1)*coverage.stepz))};
                // Empty masks own no source floor: keep editable terrain and do not
                // allocate/bind a zero coverage image over the litter material array.
                if(std::any_of(pixels.begin(),pixels.end(),[](unsigned char v){return v!=0;})){
                glGenTextures(1,&city_coverage);glBindTexture(GL_TEXTURE_2D_ARRAY,city_coverage);
                GLint unpack_alignment=4;glGetIntegerv(GL_UNPACK_ALIGNMENT,&unpack_alignment);glPixelStorei(GL_UNPACK_ALIGNMENT,1);
                glTexImage3D(GL_TEXTURE_2D_ARRAY,0,GL_R8,int(coverage.columns),int(coverage.rows),surrounding>0?2:1,0,GL_RED,GL_UNSIGNED_BYTE,pixels.data());
                glPixelStorei(GL_UNPACK_ALIGNMENT,unpack_alignment);
                glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
                }
                world_height_region_clear(&coverage);
                world_height_region_clear(&countryside);
                if(glGetError()!=GL_NO_ERROR)return false;
            }
            if(source_showcase){
                char error[256];
                const int loaded=world_height_region_load(&source_roofs,"ground/showcase.roofs",error,sizeof(error));
                if(loaded<0){std::fprintf(stderr,"[World] Invalid source weather roof region: %s\n",error);return false;}
                if(loaded>0)std::printf("[World] Source weather roofs: %ux%u, %.3f MiB; weather cover only\n",
                    source_roofs.columns,source_roofs.rows,double(source_roofs.columns)*source_roofs.rows*sizeof(float)/1048576.);
            }
            constexpr int side=513;
            size_t count=0;const auto *districts=world_atlas_districts(&count);
            if(count!=3) return false;
            std::vector<unsigned char> pixels(size_t(side)*side*(count+3)*4);
            for(size_t layer=0;layer<count;layer++) {
                const auto &d=districts[layer];
                bounds[layer*4]=float(d.xmin);bounds[layer*4+1]=float(d.zmin);
                bounds[layer*4+2]=float(1/d.width);bounds[layer*4+3]=float(1/d.depth);
                for(int z=0;z<side;z++) for(int x=0;x<side;x++) {
                    auto a=world_atlas_landscape_mask(d.xmin+d.width*x/(side-1),d.zmin+d.depth*z/(side-1));
                    size_t p=((layer*side+z)*side+x)*4;
                    // The narrow trail is evaluated analytically; a 12–35m
                    // district texel must not broaden its 3–4m walking surface.
                    pixels[p]=0;
                    pixels[p+1]=(unsigned char)std::lround(std::clamp(a.rock,0.f,1.f)*255);
                    pixels[p+2]=(unsigned char)std::lround(std::clamp(a.meadow,0.f,1.f)*255);
                    pixels[p+3]=(unsigned char)std::lround(std::clamp(a.moss,0.f,1.f)*255);
                }
            }
            double vx,vz,vw,vd;
            world_atlas_village_bounds(&vx,&vz,&vw,&vd);
            if(!std::isfinite(vx)||!std::isfinite(vz)||vw<=0||vd<=0)return false;
            village_bounds={float(vx),float(vz),float(1/vw),float(1/vd)};
            // A bounded startup bake from actual rotated building footprints.
            // This estimates obstructed sky, not secondary light transport.
            std::vector<float> roofs(size_t(side)*side,24.f),indirect_roofs=roofs;
            // Sky access has a24m reference ground. Weather cover must instead
            // start empty, particularly above the lower22.9m creek water.
            weather_roofs.assign(size_t(side)*side,0.f);weather_roof_bounds={vx,vz,vw,vd};
            size_t shelter_buildings=0,indirect_buildings=0;
            const auto stats=world_scenery_stats(scenery);
            for(size_t i=0;i<stats.source_instances;i++) {
                WorldSceneryPlacement a;
                if(!world_scenery_placement(scenery,i,&a)||!a.foundation)continue;
                bool full=a.pitch!=0||a.roll!=0||a.sx!=1||a.sy!=1||a.sz!=1;
                bool tall=(a.maximum[1]-a.minimum[1])*a.scale*a.sy>=5.5f;
                bool canopy=std::strncmp(a.alias,"village_stall_",14)==0||std::strncmp(a.alias,"purchased_shop_",15)==0||
                    std::strcmp(a.alias,"castle_residents_shelter")==0;
                if(!tall&&!canopy)continue;
                const double c=std::cos(a.yaw),s=std::sin(a.yaw);
                double xmin=1e20,xmax=-1e20,zmin=1e20,zmax=-1e20;
                for(int z=0;z<2;z++)for(int x=0;x<2;x++) {
                    double px=(x?a.maximum[0]:a.minimum[0])*a.scale,pz=(z?a.maximum[2]:a.minimum[2])*a.scale;
                    double wx=a.x+c*px+s*pz,wz=a.z-s*px+c*pz;
                    xmin=std::min(xmin,wx);xmax=std::max(xmax,wx);zmin=std::min(zmin,wz);zmax=std::max(zmax,wz);
                }
                double transformed_top=a.y+a.maximum[1]*a.scale;
                if(full){auto matrix=vw_transform::matrix({a.x,a.y,a.z,a.yaw*57.29577951308232,a.scale,a.pitch*57.29577951308232,a.roll*57.29577951308232,a.sx,a.sy,a.sz});xmin=zmin=1e20;xmax=zmax=transformed_top=-1e20;for(unsigned k=0;k<8;k++){auto point=vw_transform::point(matrix,{k&1?a.maximum[0]:a.minimum[0],k&2?a.maximum[1]:a.minimum[1],k&4?a.maximum[2]:a.minimum[2]});xmin=std::min(xmin,point[0]);xmax=std::max(xmax,point[0]);zmin=std::min(zmin,point[2]);zmax=std::max(zmax,point[2]);transformed_top=std::max(transformed_top,point[1]);}}
                if(xmax<vx||xmin>vx+vw||zmax<vz||zmin>vz+vd)continue;
                int x0=std::clamp(int(std::floor((xmin-vx)/vw*(side-1))),0,side-1);
                int x1=std::clamp(int(std::ceil((xmax-vx)/vw*(side-1))),0,side-1);
                int z0=std::clamp(int(std::floor((zmin-vz)/vd*(side-1))),0,side-1);
                int z1=std::clamp(int(std::ceil((zmax-vz)/vd*(side-1))),0,side-1);
                for(int z=z0;z<=z1;z++)for(int x=x0;x<=x1;x++) {
                    double dx=vx+vw*x/(side-1)-a.x,dz=vz+vd*z/(side-1)-a.z;
                    double lx=(c*dx-s*dz)/a.scale,lz=(s*dx+c*dz)/a.scale;
                    if(full||(lx>=a.minimum[0]&&lx<=a.maximum[0]&&lz>=a.minimum[2]&&lz<=a.maximum[2])) {
                        auto index=size_t(z)*side+x;const float top=float(transformed_top);
                        if(tall)roofs[index]=std::max(roofs[index],top);
                        indirect_roofs[index]=std::max(indirect_roofs[index],top);
                        weather_roofs[index]=std::max(weather_roofs[index],top);
                    }
                }
                if(tall)shelter_buildings++;
                indirect_buildings++;
            }
            WorldVillageSkyConfig sky_config;sky_config.columns=side;sky_config.rows=side;
            sky_config.cell_width=vw/(side-1);sky_config.cell_depth=vd/(side-1);sky_config.reference_y=25.75f;
            std::vector<uint8_t> sky;
            if(!world_village_sky_build(roofs.data(),roofs.size(),sky_config,sky))return false;
            if(!detail.build_directional(indirect_roofs,sky_config))return false;

            std::printf("[World] Village sky access: %zu building footprints, %dx%d startup bake\n",shelter_buildings,side,side);
            std::printf("[World] Village directional sky: %zu roofs/canopies, %dx%dx%d height samples, %.3f MiB\n",
                indirect_buildings,detail.directional_side,detail.directional_side,detail.directional_slices,detail.directional_bytes/1048576.);
            for(int z=0;z<side;z++)for(int x=0;x<side;x++) {
                auto a=world_atlas_village_sample(vx+vw*x/(side-1),vz+vd*z/(side-1));
                size_t p=((count*side+z)*side+x)*4;
                pixels[p]=(unsigned char)std::lround(std::clamp(a.path,0.,1.)*255);
                pixels[p+1]=(unsigned char)std::lround(std::clamp(a.paving,0.,1.)*255);
                pixels[p+2]=(unsigned char)std::lround(std::clamp(a.clearing,0.,1.)*255);
                pixels[p+3]=sky[size_t(z)*side+x];
            }
            double bx,bz,bw,bd;
            world_atlas_bandit_bounds(&bx,&bz,&bw,&bd);
            if(!std::isfinite(bx)||!std::isfinite(bz)||bw<=0||bd<=0)return false;
            camp_bounds={float(bx),float(bz),float(1/bw),float(1/bd)};
            for(int z=0;z<side;z++)for(int x=0;x<side;x++) {
                const auto a=world_atlas_bandit_sample(bx+bw*x/(side-1),bz+bd*z/(side-1));
                const size_t p=(((count+1)*side+z)*side+x)*4;
                pixels[p]=(unsigned char)std::lround(std::clamp(a.path,0.,1.)*255);
                pixels[p+1]=0; // Dirt/leaf litter, rather than village paving.
                pixels[p+2]=(unsigned char)std::lround(std::clamp(a.clearing,0.,1.)*255);
                pixels[p+3]=(unsigned char)std::lround(std::clamp(a.grass_exclusion,0.,1.)*255);
            }
            const auto *forest_pool=world_atlas_rainforest_pool();std::vector<unsigned char> forest_pixels;
            world_opening_arrival_camp::Config forest_arrival;std::string forest_error;
            std::ifstream forest_arrival_file("opening/arrival-camp.settings");
            const bool forest_has_arrival=bool(forest_arrival_file);
            if(forest_has_arrival&&!world_opening_arrival_camp::parse(forest_arrival_file,forest_arrival,forest_error))return false;
            if(!forest_pool||!forest_shelter.build_with_arrival(scenery,forest_pool->x+22,forest_pool->z+25,
                forest_has_arrival&&forest_arrival.enabled,forest_arrival.center.x,forest_arrival.center.z,side,forest_pixels,
                [](double x,double z){return world_atlas_height(x,z);}))return false;
            // The shelter field's formerly unused alpha carries authored rock
            // exposure. Same allocation and same terrain sample; no new image.
            for(int z=0;z<side;z++)for(int x=0;x<side;x++){
                const double lx=-70.+140.*x/(side-1),lz=-145.+270.*z/(side-1);
                forest_pixels[(std::size_t(z)*side+x)*4+3]=(unsigned char)std::lround((forest_shelter.arrival_region?0.:world_demo_rock_exposure(lx,lz))*255);
            }
            std::copy(forest_pixels.begin(),forest_pixels.end(),pixels.begin()+std::size_t(count+2)*side*side*4);
            std::printf("[World] Forest shelter: %u authored crowns, min%.3f mean%.3f sky, one shared513x513 layer; arrival%d Y%.2f..%.2f\n",
                forest_shelter.trees,forest_shelter.minimum_access,forest_shelter.mean_access,int(forest_shelter.arrival_region),forest_shelter.height_min,forest_shelter.height_min+forest_shelter.height_range);
            glGenTextures(1,&masks);glBindTexture(GL_TEXTURE_2D_ARRAY,masks);
            glTexImage3D(GL_TEXTURE_2D_ARRAY,0,GL_RGBA8,side,side,GLsizei(count+3),0,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
            glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
            size_t points=0;const auto *path=world_atlas_trail(&points);
            if(points!=9) return false;
            trail_bounds={float(path[0].x),float(path[0].z),float(path[0].x),float(path[0].z)};
            for(size_t i=0;i<points;i++) {
                trail[i*3]=float(path[i].x);trail[i*3+1]=float(path[i].z);trail[i*3+2]=path[i].half_width;
                trail_bounds[0]=std::min(trail_bounds[0],float(path[i].x-3));
                trail_bounds[1]=std::min(trail_bounds[1],float(path[i].z-3));
                trail_bounds[2]=std::max(trail_bounds[2],float(path[i].x+3));
                trail_bounds[3]=std::max(trail_bounds[3],float(path[i].z+3));
            }
            double xmin,zmin,width,depth;
            world_atlas_starting_bounds(&xmin,&zmin,&width,&depth);
            if(!std::isfinite(xmin)||!std::isfinite(zmin)||!std::isfinite(width)||!std::isfinite(depth)||width<=0||depth<=0)return false;
            starting_bounds={float(xmin),float(zmin),float(1/width),float(1/depth)};
            pixels.assign(size_t(side)*side*4,0);
            for(int z=0;z<side;z++)for(int x=0;x<side;x++) {
                auto a=world_atlas_starting_sample(xmin+width*x/(side-1),zmin+depth*z/(side-1));
                size_t p=(size_t(z)*side+x)*4;
                pixels[p]=(unsigned char)std::lround(std::clamp(a.path,0.,1.)*255);
                pixels[p+1]=(unsigned char)std::lround(std::clamp(a.paving,0.,1.)*255);
                pixels[p+2]=(unsigned char)std::lround(std::clamp(a.clearing,0.,1.)*255);
            }
            glGenTextures(1,&starting);glBindTexture(GL_TEXTURE_2D,starting);
            glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,side,side,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
            // Source terrain uses its eight authored PBR layers. Retain the
            // masks and weather roof metadata above without resident copies
            // of the atlas village stone, litter and ground-history textures.
            if(source_showcase&&source_pbr)return masks&&starting;
            if(!detail.load_litter("ground/terrain_dirt_nor.png","ground/terrain_dirt_rgh.png",
                visual_surface_finish?"ground/forest_litter_color.png":nullptr,visual_surface_finish?"ground/forest_litter_normal.png":nullptr,visual_surface_finish?"ground/forest_litter_material.png":nullptr))return false;
            // Editable source projects have no eight-layer source terrain. They
            // still need the real earth normal/ARM maps, but no atlas village bake.
            if(source_showcase||city_showcase)return masks&&detail.litter&&starting;
            if(!detail.load_history())return false;
            double origin_x,origin_z;world_atlas_village_origin(&origin_x,&origin_z);
            castle_lights_ready=world_castle_lights_build(origin_x,origin_z,castle_lights);
            tavern_lighting_ready=world_tavern_lighting_build(origin_x,origin_z,tavern_lighting);
            ground_history_bounds={float(origin_x+world_village_ground_min_x),float(origin_z+world_village_ground_min_z),
                float(1./world_village_ground_width),float(1./world_village_ground_depth)};
            std::printf("[World] Village ground history: %dx%d linear weights, %.3f MiB with mips; litter normal/ARM share sampler13\n",
                world_village_ground_side,world_village_ground_side,detail.history_bytes/1048576.);
            return masks&&detail.litter&&starting&&load_village_pbr();
        }
        void bind_forest_shelter(Program &p) {
            p.i("uForestShelterEnabled",forest_shelter_enabled&&masks&&forest_shelter.trees);
            glUniform4fv(p.at("uForestShelterBounds"),1,forest_shelter.bounds.data());
            p.v2("uForestShelterHeights",forest_shelter.height_min,forest_shelter.height_range);
        }
        void bind_shelter(Program &p) {
            bind_forest_shelter(p);
            glActiveTexture(GL_TEXTURE12);glBindTexture(GL_TEXTURE_2D_ARRAY,studio_surface_gpu.choose(masks));p.i("uVillageSkyMask",12);
            p.i("uVillageSkyEnabled",masks&&!city_showcase?1:0);glUniform4fv(p.at("uVillageSkyBounds"),1,village_bounds.data());
            p.v2("uVillageSkyHeights",24.f,18.f);
            p.v3("uVillageBounceAlbedo",village_pbr?V3{.16384777f,.12907667f,.09379962f}:V3{0,0,0});
            glActiveTexture(GL_TEXTURE13);glBindTexture(GL_TEXTURE_3D,detail.directional);p.i("uVillageSkyDirectional",13);
            glActiveTexture(GL_TEXTURE14);glBindTexture(GL_TEXTURE_3D,detail.up);p.i("uVillageSkyUp",14);
            p.i("uVillageDirectionalEnabled",directional_enabled&&detail.directional&&detail.up);
            glUniform3fv(p.at("uVillageDirectionalHeights"),1,detail.heights.data());
            p.i("uCastleLightsEnabled",castle_lights_ready);
            glUniform4fv(p.at("uCastleLightPositionRadius[0]"),WorldCastleLights::count,castle_lights.position_radius[0].data());
            glUniform3fv(p.at("uCastleLightColor"),1,castle_lights.color.data());
            glUniform4fv(p.at("uCastleLightBounds"),1,castle_lights.bounds.data());
            glUniform2fv(p.at("uCastleLightHeights"),1,castle_lights.heights.data());
            p.i("uTavernReturnEnabled",tavern_lighting_ready&&tavern_return_enabled);
            glUniform4fv(p.at("uTavernReturnBounds"),1,tavern_lighting.bounds.data());
            glUniform2fv(p.at("uTavernReturnHeights"),1,tavern_lighting.heights.data());
            glUniform3fv(p.at("uTavernReturnRoof"),1,tavern_lighting.roof.data());
            glUniform3fv(p.at("uTavernReturnIrradiance"),1,tavern_lighting.irradiance.data());
        }
        void bind(Program &p) {
            p.i("uForestGroundReady",detail.litter?1:0);
            bind_forest_shelter(p);
            p.v3("uVillageBounceAlbedo",village_pbr?V3{.16384777f,.12907667f,.09379962f}:V3{0,0,0});
            glActiveTexture(GL_TEXTURE10);glBindTexture(GL_TEXTURE_2D_ARRAY,village_pbr);p.i("uVillagePBR",10);p.i("uVillagePBRReady",village_pbr?1:0);
            p.i("uVillageSkyEnabled",masks&&!city_showcase?1:0);
            glActiveTexture(GL_TEXTURE12);glBindTexture(GL_TEXTURE_2D_ARRAY,studio_surface_gpu.choose(masks));p.i("uLandscape",12);
            glActiveTexture(GL_TEXTURE13);glBindTexture(GL_TEXTURE_2D_ARRAY,detail.litter);p.i("uLitterPBR",13);p.i("uForestLitter",detail.forest_litter?1:0);
            glActiveTexture(GL_TEXTURE14);glBindTexture(GL_TEXTURE_2D,detail.history);p.i("uVillageGroundMask",14);
            glUniform4fv(p.at("uVillageGroundBounds"),1,ground_history_bounds.data());
            p.i("uVillageGroundReady",history_enabled&&detail.history);
            p.i("uVillageStochasticPaving",history_enabled&&detail.history);
            glUniform4fv(p.at("uDistricts[0]"),3,bounds.data());
            glUniform4fv(p.at("uVillageBounds"),1,village_bounds.data());p.i("uHasVillage",masks&&!city_showcase?1:0);
            glUniform4fv(p.at("uBanditBounds"),1,camp_bounds.data());p.i("uHasBandit",masks?1:0);
            glUniform3fv(p.at("uTrail[0]"),9,trail.data());
            glUniform4fv(p.at("uTrailBounds"),1,trail_bounds.data());
            glActiveTexture(GL_TEXTURE15);glBindTexture(GL_TEXTURE_2D,starting);p.i("uStartingMask",15);
            glUniform4fv(p.at("uStartingBounds"),1,starting_bounds.data());p.i("uHasStarting",starting?1:0);
            // The city uses a visual ground mesh with exact source materials.
            // Reuse the disabled source litter array's unit and sampler type;
            // no extra texture unit or old-project terrain behavior changes.
            const int coverage_unit=detail.litter?10:13; // city mode never owns atlas village PBR
            p.i("uCityGroundMaskReady",city_coverage?1:0);p.i("uCityGroundCoverage",coverage_unit);
            glUniform4fv(p.at("uCityGroundBounds"),1,city_ground_bounds.data());
            p.i("uCountrysideGroundMaskReady",city_coverage&&countryside_ground_ready?1:0);
            glUniform4fv(p.at("uCountrysideGroundBounds"),1,countryside_ground_bounds.data());
            if(city_coverage){glActiveTexture(GL_TEXTURE0+coverage_unit);glBindTexture(GL_TEXTURE_2D_ARRAY,city_coverage);}

        }
        void release() {world_height_region_clear(&source_roofs);detail.release();GLuint textures[]={masks,starting,village_pbr,city_coverage};glDeleteTextures(4,textures);}
    };

    struct GrassGPU {
        GLuint vao = 0, vbo = 0, ebo = 0, instances = 0;
        GLsizei count = 0;
        size_t blades = 0,culled_frustum=0,culled_wall=0,density_rejected=0;
        double x = 0, z = 0;
        std::vector<float> staging;
        std::vector<unsigned char> procedural_keep,density_keep;
        float classified_density_scale=-1.f;int classified_budget_tier=-1;
        std::uint64_t classification_revision=0;
        std::uint64_t classification_refreshes=0,classified_seeds=0;
        bool has_classification=false;
        void init() {
            staging.reserve(WORLD_COVER_GRASS_MAX_CAP*2*6);
            procedural_keep.reserve(WORLD_COVER_GRASS_MAX_CAP*2);
            density_keep.reserve(WORLD_COVER_GRASS_MAX_CAP*2);
            struct Vertex {
                float x, y, z, nx, ny, nz, u, v, tx, ty, tz, tw;
            };
            std::vector<Vertex> vertices;
            std::vector<unsigned> indices;
            for (int leaf = 0; leaf < 7; leaf++) {
                float angle = leaf * 2.399963f, c = std::cos(angle), sn = std::sin(angle);
                unsigned start = unsigned(vertices.size());
                for (int row = 0; row < 4; row++) {
                    float t = row / 3.f;
                    float width = (.008f + .002f * std::sin(leaf * 1.9f)) * (1 - t);
                    float bend = t * t * (.10f + .05f * std::sin(leaf * 2.1f));
                    for (int side = 0; side < 2; side++) {
                        float across = (side ? 1.f : -1.f) * width;
                        float fold = side ? .15f : -.15f;
                        V3 n = norm({sn + c * fold, .55f + t * .6f, c - sn * fold});
                        float height = .23f + .04f * std::sin(leaf * 2.4f);
                        vertices.push_back({
                            c * across + sn * bend, t * height, -sn * across + c * bend, n.x, n.y, n.z, float(side), t, c,
                                0, -sn, 1
                        });
                    }
                }
                for (unsigned row = 0; row < 3; row++) {
                    unsigned a = start + row * 2;
                    indices.insert(indices.end(), {a, a + 1, a + 3, a, a + 3, a + 2});
                }
            }
            glGenVertexArrays(1, &vao);
            glGenBuffers(1, &vbo);
            glGenBuffers(1, &ebo);
            glGenBuffers(1, &instances);
            glBindVertexArray(vao);
            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);
            const int sizes[] = {3, 3, 2, 4}, offsets[] = {0, 3, 6, 8};
            for (int i = 0; i < 4; i++) {
                glEnableVertexAttribArray(i);
                glVertexAttribPointer(i, sizes[i], GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)(uintptr_t)(offsets[i] * sizeof(float)));
            }
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned), indices.data(), GL_STATIC_DRAW);
            glBindBuffer(GL_ARRAY_BUFFER, instances);
            glEnableVertexAttribArray(4);
            glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, 6 * sizeof(float), nullptr);
            glVertexAttribDivisor(4, 1);
            glEnableVertexAttribArray(5);
            glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)(uintptr_t)(4 * sizeof(float)));
            glVertexAttribDivisor(5, 1);
            glEnableVertexAttribArray(6);
            glVertexAttribPointer(6,1,GL_FLOAT,GL_FALSE,6*sizeof(float),(void *)(uintptr_t)(5*sizeof(float)));
            glVertexAttribDivisor(6,1);
            glBindVertexArray(0);
            count = GLsizei(indices.size());
        }

        void upload(const WorldCoverField &f,const std::vector<float> &fades,const Camera &cam,std::uint64_t root_revision) {
            auto &data=staging;data.clear();culled_frustum=culled_wall=density_rejected=0;
            // Opacity-only fades keep root bytes/order unchanged. Reclassify
            // only after replacement/reset/compaction, never for fade progress.
            if(!has_classification||classification_revision!=root_revision||procedural_keep.size()!=f.count||
               classified_density_scale!=foliage_density_scale||classified_budget_tier!=int(foliage_budget_tier)) {
                procedural_keep.resize(f.count);density_keep.resize(f.count);
                classification_refreshes++;classified_seeds+=f.count;
                for(size_t i=0;i<f.count;i++) {
                    const auto &a=f.instances[i];
                    const auto keep=world_grass_density::classify(a.yaw,a.scale,foliage_density_scale,int(foliage_budget_tier));
                    procedural_keep[i]=keep.blades;density_keep[i]=keep.density;
                }
                classification_revision=root_revision;classified_density_scale=foliage_density_scale;
                classified_budget_tier=int(foliage_budget_tier);has_classification=true;
            }
            const auto view=world_vegetation_visibility_view(cam.x,cam.y,cam.z,cam.yaw,cam.pitch,cam.aspect,std::tan(cam.fov*.5f));
            double ox,oz;world_atlas_village_origin(&ox,&oz);
            const auto wall=world_castle_occlusion_view(cam.x-ox,cam.y,cam.z-oz);
            for (size_t i = 0; i < f.count; i++) {
                const auto &a = f.instances[i];
                // Short inexpensive blades form a meadow understory between
                // photographed tufts. A rooted blade keeps the same seeded
                // selection across cache-anchor changes at every distance.
                if(fades[i]<=0.f)continue;
                if(!density_keep[i]){density_rejected++;continue;}
                if(!procedural_keep[i])continue;
                const double wx=f.anchor_x+a.x,wz=f.anchor_z+a.z;
                const double radius=.5*a.scale+.25,top=a.y+.27*a.scale+.15;
                if(world_vegetation_sphere_outside(view,wx,(a.y+top)*.5,wz,radius,90.*foliage_detail_scale)){culled_frustum++;continue;}
                if(studio_castle_culling&&world_castle_wall_occludes(wall,wx-ox-radius,a.y-.05,wz-oz-radius,wx-ox+radius,top,wz-oz+radius)){culled_wall++;continue;}
                data.insert(data.end(), {a.x, a.y, a.z, a.scale, a.yaw,fades[i]});
            }
            glBindBuffer(GL_ARRAY_BUFFER, instances);
            glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), data.data(), GL_STREAM_DRAW);
            blades = data.size()/6;
            x = f.anchor_x;
            z = f.anchor_z;
        }

        void draw(Program &p, const Camera &c) {
            if (!blades)
                return;
            p.v3("uOffset", {float(x - c.x), float(-c.y), float(z - c.z)});
            p.v2("uWorldAnchor", float(x), float(z));
            p.i("uVegetation", 2);
            p.i("uLodLevel", -1);
            p.i("uHasBase", 0);
            p.i("uHasNormal", 0);
            p.i("uHasARM", 0);
            p.i("uGrass", 1);
            p.i("uInstanceFade",1);
            p.f("uAlphaCutoff", .01f);
            p.f("uRoughness", .9f);
            p.f("uMetallic", 0.f);
            glUniform4f(p.at("uBaseFactor"), .17f, .245f, .075f, 1.f);
            p.i("uRootedGrassWind",1);p.f("uTreeHeight",.27f);
            p.f("uWind", .45f);
            glBindVertexArray(vao);
            glDrawElementsInstanced(GL_TRIANGLES, count, GL_UNSIGNED_INT, nullptr, GLsizei(blades));
            p.i("uInstanceFade",0);p.i("uRootedGrassWind",0);
        }

        void release() {
            glDeleteVertexArrays(1, &vao);
            GLuint buffers[] = {vbo, ebo, instances};
            glDeleteBuffers(3, buffers);
        }
    };

    struct Dressing {
        static constexpr size_t scanned_batch_capacity=4096,scanned_group_capacity=16384;
        struct Group {
            WorldModelAsset *asset = nullptr,*shadow_asset=nullptr,*shadow_cull_asset=nullptr;
            std::array<WorldModelAsset*,3> overflow_assets={};
            WorldCoverKind kind = WORLD_COVER_TREES;
            unsigned variant = 0, variants = 1;
            float base_near_distance=0,base_far_distance=0,near_distance = 0, far_distance = 0;
            size_t base_cap=0;
            size_t cap = 0;
            bool shadow = false,plain_grass_materials=false;
            int vegetation = 0, lod = -1;
            float tree_height=25.f,bound_radius=1.f,bound_top=1.f;
            std::vector<WorldModelInstance> visible, uploaded, shadow_selected,shadow_uploaded;
            std::vector<float> visible_fades,uploaded_fades;
            std::vector<std::pair<double, size_t>> selected;
        };
        WorldCover *cover = nullptr;
        WorldGrassTransition grass_transition;
        bool shadow_lod=true;
        bool tree_shadow_culling=std::getenv("VALWOOD_QA_DISABLE_TREE_SHADOW_CULL")==nullptr;
        uint64_t tree_shadow_candidates=0,tree_shadow_rejected=0;
        GrassGPU grass;
        std::array<WorldCoverField, WORLD_COVER_KIND_COUNT> fields = {};
        std::vector<unsigned char> tree_species;
        std::vector<Group> groups;
        std::array<WorldModelAsset*,4> reflection_assets={};
        std::array<std::vector<WorldModelInstance>,4> reflection_instances,reflection_uploaded;
        std::array<float,4> reflection_heights={25,22,10,3.2f};
        Camera selection_camera;
        bool has_selection=false;
        float selected_detail_scale=0.f,selected_density_scale=-1.f;
        int selected_budget_tier=-1;
        size_t scanned_near_count=0,scanned_mid_count=0;
        uint64_t density_rejected=0,specialized_batches=0,specialized_instances=0;
        size_t scanned_grass_count = 0;
        size_t grass_blend_updates=0,peak_blended_grass_roots=0;
        size_t peak_scanned_group_candidates=0,scanned_grass_cap_drops=0,scanned_culled_frustum=0,scanned_culled_wall=0;
        size_t tree_count = 0, near_tree_count = 0, mid_tree_count = 0, far_tree_count = 0;
        uint64_t tree_dispatches=0,grass_dispatches=0,tree_fallbacks=0,grass_fallbacks=0;
        bool add(const char *path, WorldCoverKind kind, unsigned variant, unsigned variants,
                 float lo, float hi, size_t cap, bool shadow, int vegetation) {
            Group g;
            g.asset = world_model_load_budgeted(path,environment_texture_edge);
            // Two bounded draw batches retain both rooted generations during
            // a grass transition. Embedded images share the texture cache.
            if(kind==WORLD_COVER_GRASS)for(size_t i=0;i<g.overflow_assets.size();++i)
                if(cap>(i+1)*scanned_batch_capacity)g.overflow_assets[i]=world_model_load_budgeted(path,environment_texture_edge);
            g.plain_grass_materials=kind==WORLD_COVER_GRASS&&world_model_plain_materials(g.asset);
            for(auto *a:g.overflow_assets)if(a&&!world_model_plain_materials(a))g.plain_grass_materials=false;
            float minimum[3],maximum[3];
            if(world_model_bounds(g.asset,minimum,maximum)) {
                g.tree_height=maximum[1]-minimum[1];g.bound_top=maximum[1];
                g.bound_radius=std::hypot(std::max(std::abs(minimum[0]),std::abs(maximum[0])),
                                          std::max(std::abs(minimum[2]),std::abs(maximum[2])));
            }
            g.kind = kind; g.variant = variant; g.variants = variants;
            g.base_near_distance=g.near_distance = lo; g.base_far_distance=g.far_distance = hi; g.base_cap=g.cap = cap;
            g.shadow = shadow; g.vegetation = vegetation;
            g.visible.reserve(cap); g.uploaded.reserve(cap);g.shadow_selected.reserve(cap);g.shadow_uploaded.reserve(cap);
            if(kind==WORLD_COVER_GRASS){g.visible_fades.reserve(cap);g.uploaded_fades.reserve(cap);}
            g.selected.reserve(kind == WORLD_COVER_TREES ? WORLD_COVER_TREE_CAP :
                               kind==WORLD_COVER_GRASS?std::max(cap,size_t(WORLD_COVER_GRASS_MAX_CAP*2/12+512)):size_t(WORLD_COVER_FERN_MAX_CAP));
            groups.push_back(std::move(g));
            return groups.back().asset != nullptr&&
                (kind!=WORLD_COVER_GRASS||std::all_of(groups.back().overflow_assets.begin(),groups.back().overflow_assets.end(),[](auto *a){return a!=nullptr;}));
        }
        static bool upload(Group &g,const std::vector<WorldModelInstance> &instances,const std::vector<float> &fades) {
            if(g.kind==WORLD_COVER_GRASS) {
                if(instances.size()>scanned_group_capacity||instances.size()!=fades.size())return false;
                bool ready=true;
                for(size_t batch=0;batch<4;++batch){
                    auto *asset=batch?g.overflow_assets[batch-1]:g.asset;
                    const size_t start=batch*scanned_batch_capacity;
                    const size_t count=start<instances.size()?std::min(scanned_batch_capacity,instances.size()-start):0;
                    if(!asset){if(count)return false;continue;}
                    ready=world_model_upload_fading_instances(asset,count?instances.data()+start:nullptr,count?fades.data()+start:nullptr,count)&&ready;
                }
                return ready;
            }
            bool ready=world_model_upload_instances(g.asset,instances.data(),instances.size());
            if(g.shadow_asset)ready=world_model_upload_instances(g.shadow_asset,instances.data(),instances.size())&&ready;
            if(g.shadow_cull_asset)ready=world_model_upload_instances(g.shadow_cull_asset,instances.data(),instances.size())&&ready;
            if(ready&&(g.shadow_asset||g.shadow_cull_asset))g.shadow_uploaded.assign(instances.begin(),instances.end());
            return ready;
        }
        bool init() {
            // Explicit editor sessions keep the streamed planting assets ready.
            // Showcase cover remains authored-only: no baseline forest leaks in.
            if(source_showcase&&!landscape_authoring_session&&(!studio_document||studio_document->ecology.empty()))return true;
            cover = world_cover_init();
            shadow_lod=std::getenv("VALWOOD_DISABLE_SHADOW_LOD")==nullptr;
            tree_species.reserve(WORLD_COVER_TREE_CAP);
            grass.init();
            groups.reserve(24);
            bool ok = cover != nullptr&&world_cover_set_quality(cover,int(foliage_budget_tier));
            const char *species[] = {"pine", "fir","broadleaf","pine_young"};
            for(unsigned i=0;i<4;i++) {
                char path[128];std::snprintf(path,sizeof(path),"models/cinematic_%s_far.glb",species[i]);
                reflection_assets[i]=world_model_load_budgeted(path,environment_texture_edge);
                reflection_instances[i].reserve(4096);reflection_uploaded[i].reserve(4096);
                ok=reflection_assets[i]&&ok;
            }
            const char *lods[] = {"near", "mid", "far"};
            const float starts[] = {0.f, WORLD_TREE_NEAR_FADE_BEGIN, 215.f}, ends[] = {WORLD_TREE_NEAR_FADE_END, 265.f, 1680.f};
            const size_t caps[4][3]={{28,200,2700},{28,200,2700},{28,180,1200},{12,80,400}};
            for (unsigned type = 0; type < 4; type++)
                for (unsigned lod = 0; lod < 3; lod++) {
                    char path[128];
                    std::snprintf(path, sizeof(path), "models/cinematic_%s_%s.glb", species[type], lods[lod]);
                    ok = add(path, WORLD_COVER_TREES, type, 4, starts[lod], ends[lod], caps[type][lod], lod < 2,
                             lod == 2 ? 4 : 1) && ok;
                    groups.back().lod=int(lod);
                    groups.back().tree_height=reflection_heights[type];
                    if(lod==1) {
                        groups.back().shadow_cull_asset=world_model_load_budgeted(path,environment_texture_edge);
                        ok=groups.back().shadow_cull_asset&&ok;
                    }
                    if(lod==0) {
                        std::snprintf(path,sizeof(path),"models/cinematic_%s_mid.glb",species[type]);
                        groups.back().shadow_asset=world_model_load_budgeted(path,environment_texture_edge);
                        ok=groups.back().shadow_asset&&ok;
                    }
                }
            for (unsigned variant = 0; variant < 3; variant++) {
                char path[128];
                std::snprintf(path, sizeof(path), "models/cinematic_rock_%u.glb", variant);
                ok = add(path, WORLD_COVER_ROCKS, variant, 3, 0, 200, WORLD_COVER_ROCK_CAP, true, 0) && ok;
                std::snprintf(path, sizeof(path), "models/woodland_fern_%u.glb", variant);
                ok = add(path, WORLD_COVER_FERNS, variant, 3, 0, 110, WORLD_COVER_FERN_MAX_CAP, false, 3) && ok;
            }
            for(unsigned variant=0;variant<3;variant++) {
                char path[128];std::snprintf(path,sizeof(path),"models/cinematic_grass_%u.glb",variant);
                char mid_path[160];std::snprintf(mid_path,sizeof(mid_path),"models/optimized_grass/tuft_%u_mid.glb",variant);
                const bool paired=variant<2&&std::filesystem::exists(mid_path)&&std::getenv("VALWOOD_QA_DISABLE_GRASS_LOD")==nullptr;
                if(paired){
                    ok=add(path,WORLD_COVER_GRASS,variant*4,12,0,world_ground_cover_quality::grass_lod_end,scanned_group_capacity,false,5)&&ok;
                    groups.back().lod=0;
                    ok=add(mid_path,WORLD_COVER_GRASS,variant*4,12,world_ground_cover_quality::grass_lod_begin,70,scanned_group_capacity,false,5)&&ok;
                    groups.back().lod=1;
                }else ok=add(path,WORLD_COVER_GRASS,variant*4,12,0,70,scanned_group_capacity,false,5)&&ok;
            }
            return ok;
        }
        static uint32_t root_hash(const WorldCoverField &, const WorldModelInstance &a) {
            // These world-seeded attributes are identical across cover anchors.
            // Reconstructing global coordinates from local floats can change a
            // quantized position at a cell boundary and switch species in place.
            return world_ground_cover_quality::generated_seed(a.yaw,a.scale);
        }
        static unsigned species(uint32_t root,float wet) {
            const unsigned choice=root%10;
            if(wet>0&&float(root>>8)*(1.f/16777216.f)<wet)
                return choice<5?2:(choice<9?1:3);
            return choice<4?0:(choice<7?1:(choice<9?2:3));
        }
        void update(const Camera &cam,double seconds=1./60.,bool editor_loading=false) {
            if(!cover)return;
            world_cover_set_quality(cover,int(foliage_budget_tier));
            world_cover_set_detail_scale(cover,foliage_detail_scale);
            world_cover_update(cover, cam.x, cam.z);
            const auto pump_started=std::chrono::steady_clock::now();
            for(unsigned batch=0;batch<(editor_loading?64u:1u);++batch){
                if(!world_cover_pump(cover,4096))break;
                if(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-pump_started).count()>=4.)break;
            }
            WorldCoverField field = {};
            bool grass_changed=grass_transition.advance(seconds);
            bool changed_field=grass_changed;
            while (world_cover_poll(cover, &field)) {
                changed_field=true;
                if(field.kind==WORLD_COVER_GRASS){grass_transition.replace(field);grass_changed=true;}
                else fields[field.kind] = field;
                if(field.kind==WORLD_COVER_TREES) {
                    tree_species.clear();
                    for(size_t i=0;i<field.count;i++) {
                        const auto &a=field.instances[i];const uint32_t root=root_hash(field,a);
                        float wet=world_atlas_rainforest_weight(field.anchor_x+a.x,field.anchor_z+a.z);int habitat=-1;
                        if(studio_document){habitat=world_cover_region_preset(studio_document->ecology.data(),studio_document->ecology.size(),field.anchor_x+a.x,field.anchor_z+a.z);if(habitat>=0)wet=habitat==2?1.f:0.f;}
                        tree_species.push_back(static_cast<unsigned char>(habitat==2?2u:species(root,wet)));
                    }
                }
                if(field.kind==WORLD_COVER_TREES||field.kind==WORLD_COVER_ROCKS){
                    std::array<world_cover_collision::Shape,4> shapes={};
                    std::array<bool,4> found={};
                    const size_t shape_count=field.kind==WORLD_COVER_TREES?4:3;
                    for(const auto &g:groups)if(g.kind==field.kind&&g.variant<shape_count&&(field.kind!=WORLD_COVER_TREES||g.lod==0)){
                        auto &shape=shapes[g.variant];
                        found[g.variant]=world_model_bounds(g.asset,shape.minimum.data(),shape.maximum.data());
                        shape.kind=field.kind==WORLD_COVER_TREES?world_cover_collision::Kind::Trunk:world_cover_collision::Kind::Rock;
                        if(field.kind==WORLD_COVER_TREES){shape.trunk_radius=std::array<float,4>{.28f,.32f,.42f,.2f}[g.variant];shape.minimum[1]=0;shape.maximum[1]=std::min(5.f,shape.maximum[1]);}
                        else shape.y_offset=-(g.variant==0?1.8f:g.variant==1?2.5f:3.2f)*.20f;
                    }
                    std::vector<unsigned char> variants;if(field.kind==WORLD_COVER_TREES)variants=tree_species;
                    else{variants.reserve(field.count);for(size_t i=0;i<field.count;++i)variants.push_back(static_cast<unsigned char>(root_hash(field,field.instances[i])%3));}
                    std::string error;
                    const bool have_shapes=std::all_of(found.begin(),found.begin()+shape_count,[](bool good){return good;});
                    generated_cover_kind_ready[field.kind==WORLD_COVER_TREES?0:1]=have_shapes&&generated_cover_collision.publish(field,shapes.data(),shape_count,variants.data(),variants.size(),error);
                    generated_cover_collision_ready=generated_cover_kind_ready[0]&&generated_cover_kind_ready[1];
                    if(!generated_cover_collision_ready)std::fprintf(stderr,"[Cover collision] %s\n",error.empty()?"Missing near-model collision bounds":error.c_str());
                }
            }
            if(grass_changed) {
                fields[WORLD_COVER_GRASS]=grass_transition.field();
                grass_blend_updates++;
                peak_blended_grass_roots=std::max(peak_blended_grass_roots,fields[WORLD_COVER_GRASS].count);
            }
            select(cam,changed_field,grass_changed);
        }
        // Loading previews may change the view selection without pumping the
        // cover generator, advancing rooted fades or changing play counters.
        void select(const Camera &cam,bool changed_field=false,bool grass_changed=false,bool record_stats=true) {
            if(!cover)return;
            if(selected_budget_tier!=int(foliage_budget_tier)){
                selected_budget_tier=int(foliage_budget_tier);has_selection=false;grass.has_classification=false;
                const auto budget=world_ground_cover_quality::profile(foliage_budget_tier);
                for(auto &g:groups){
                    if(g.kind==WORLD_COVER_FERNS)g.cap=budget.ferns;
                    if(g.kind==WORLD_COVER_GRASS)g.cap=std::clamp(size_t((budget.grass*2+11)/12+512),scanned_batch_capacity,scanned_group_capacity);
                }
            }
            if(selected_detail_scale!=foliage_detail_scale){
                selected_detail_scale=foliage_detail_scale;has_selection=false;
                for(auto &g:groups){
                    const bool foliage=g.kind==WORLD_COVER_TREES||g.kind==WORLD_COVER_GRASS||g.kind==WORLD_COVER_FERNS;
                    const float scale=foliage?foliage_detail_scale:1.f;
                    g.near_distance=g.base_near_distance*scale;
                    g.far_distance=g.kind==WORLD_COVER_TREES&&g.lod==2?g.base_far_distance:g.base_far_distance*scale;
                    // Retain both meshes beyond the shader's overlap while the
                    // guarded selection cache permits sub-metre camera motion.
                    if(g.kind==WORLD_COVER_GRASS&&g.lod==0)g.far_distance+=1.f;
                    if(g.kind==WORLD_COVER_GRASS&&g.lod==1)g.near_distance=std::max(0.f,g.near_distance-1.f);
                    if(g.kind==WORLD_COVER_TREES&&g.lod<2)g.cap=std::min(size_t(4096),size_t(std::ceil(g.base_cap*scale*scale)));
                }
            }
            if(selected_density_scale!=foliage_density_scale){selected_density_scale=foliage_density_scale;has_selection=false;}
            const float selection_turn_scale=std::max(1.f,cam.aspect/(16.f/9.f));
            // The frustum already has a 40m guard and LODs overlap by20/50m.
            // Preserve selections through sub-meter motion / sub-degree turns;
            // draw matrices, wind, dither and temporal resolve still run each frame.
            if(!changed_field&&has_selection&&cam.aspect==selection_camera.aspect&&
               std::hypot(cam.x-selection_camera.x,cam.z-selection_camera.z)<.75&&
               std::fabs(cam.y-selection_camera.y)<.25&&
               std::fabs(std::remainder(cam.yaw-selection_camera.yaw,6.2831853f))<.01f/selection_turn_scale&&
               std::fabs(cam.pitch-selection_camera.pitch)<.005f/selection_turn_scale) return;
            selection_camera=cam;has_selection=true;
            // Reuse the existing guarded selection cache. Camera motion only
            // rebuilds bounded visible grass batches; root fades/ranks stay put.
            (void)grass_changed; // Preview-call compatibility; root revision owns classification invalidation.
            grass.upload(fields[WORLD_COVER_GRASS],grass_transition.fades(),cam,grass_transition.structure_revision());
            const auto grass_view=world_vegetation_visibility_view(cam.x,cam.y,cam.z,cam.yaw,cam.pitch,cam.aspect,std::tan(cam.fov*.5f));
            double village_x,village_z;world_atlas_village_origin(&village_x,&village_z);
            const auto grass_wall=world_castle_occlusion_view(cam.x-village_x,cam.y,cam.z-village_z);
            scanned_culled_frustum=scanned_culled_wall=0;density_rejected=grass.density_rejected;
            tree_count = near_tree_count = mid_tree_count = far_tree_count = scanned_grass_count = 0;
            V3 forward = cam.forward(), right = cam.right();
            const auto *pool=world_atlas_rainforest_pool();
            const bool pool_mirror=std::hypot(cam.x-pool->x,cam.z-pool->z)<220.;
            // Dispatch the same candidate pairs in original source order. A
            // malformed/extended group layout retains the old per-group scan.
            std::array<std::array<Group*,3>,4> tree_groups={};
            std::array<std::array<Group*,2>,12> grass_groups={};
            bool tree_dispatch=tree_species.size()==fields[WORLD_COVER_TREES].count,grass_dispatch=true;
            double grass_lo=1e300,grass_hi=-grass_lo;
            for(auto &g:groups) {
                g.selected.clear();
                if(g.kind==WORLD_COVER_TREES) {
                    if(g.variant>=4||g.variants!=4||g.lod<0||g.lod>=3)tree_dispatch=false;
                    else {auto &slot=tree_groups[g.variant][std::size_t(g.lod)];if(slot)tree_dispatch=false;else slot=&g;}
                } else if(g.kind==WORLD_COVER_GRASS) {
                    if(g.variant>=12||g.variants!=12||g.lod>1||!std::isfinite(g.near_distance)||!std::isfinite(g.far_distance))grass_dispatch=false;
                    else {
                        auto &slot=grass_groups[g.variant][g.lod==1?1:0];if(slot)grass_dispatch=false;else slot=&g;
                        grass_lo=std::min(grass_lo,double(g.near_distance*g.near_distance));
                        grass_hi=std::max(grass_hi,double(g.far_distance*g.far_distance));
                    }
                }
            }
            tree_dispatches+=tree_dispatch;grass_dispatches+=grass_dispatch;
            tree_fallbacks+=!tree_dispatch;grass_fallbacks+=!grass_dispatch;
            auto point_visible=[&](const WorldModelInstance &a,double x,double z) {
                const double horizontal=x*forward.x+z*forward.z;
                double depth=std::max(horizontal+(a.y+12*a.scale-cam.y)*forward.y,
                                      horizontal-(a.y+12*a.scale+cam.y)*forward.y);
                if(pool_mirror)depth=std::max(depth,horizontal-(a.y+12*a.scale+cam.y-2*pool->surface_y)*forward.y);
                const double across=x*right.x+z*right.z;
                return !(depth < -35 || std::fabs(across) > std::max(0.,depth)*cam.aspect*std::tan(double(cam.fov)*.5) + 40);
            };
            if(tree_dispatch) {
                const auto &f=fields[WORLD_COVER_TREES];
                for(std::size_t i=0;i<f.count;i++) {
                    const auto &a=f.instances[i];const auto type=tree_species[i];
                    if(type>=tree_groups.size())continue;
                    const double x=f.anchor_x+a.x-cam.x,z=f.anchor_z+a.z-cam.z,d2=x*x+z*z;
                    for(auto *g:tree_groups[type])if(g) {
                        if(d2>=g->far_distance*g->far_distance||d2<g->near_distance*g->near_distance)continue;
                        if(!g->shadow&&!point_visible(a,x,z))continue;
                        g->selected.push_back({d2,i});
                    }
                }
            }
            if(grass_dispatch) {
                const auto &f=fields[WORLD_COVER_GRASS];const auto &fades=grass_transition.fades();
                for(std::size_t i=0;i<f.count;i++) {
                    if(fades[i]<=0.f||!grass.density_keep[i])continue;
                    const auto &a=f.instances[i];
                    const double x=f.anchor_x+a.x-cam.x,z=f.anchor_z+a.z-cam.z,d2=x*x+z*z;
                    if(d2>=grass_hi||d2<grass_lo)continue;
                    const auto seed=root_hash(f,a);
                    for(auto *g:grass_groups[seed%grass_groups.size()]){if(!g)continue;
                    if(d2>=g->far_distance*g->far_distance||d2<g->near_distance*g->near_distance)continue;
                    const double wx=f.anchor_x+a.x,wz=f.anchor_z+a.z;
                    const double radius=g->bound_radius*a.scale*1.4+.25,top=a.y+g->bound_top*a.scale*1.4+.15;
                    if(world_vegetation_sphere_outside(grass_view,wx,(a.y+top)*.5,wz,
                        std::hypot(radius,(top-a.y)*.5),70.*foliage_detail_scale)){scanned_culled_frustum++;continue;}
                    if(studio_castle_culling&&world_castle_wall_occludes(grass_wall,wx-village_x-radius,a.y-.05,wz-village_z-radius,
                        wx-village_x+radius,top,wz-village_z+radius)){scanned_culled_wall++;continue;}
                    if(!g->shadow&&!point_visible(a,x,z))continue;
                    g->selected.push_back({d2,i});
                    }
                }
            }
            scanned_near_count=scanned_mid_count=0;
            for (auto &g : groups) {
                const auto &f = fields[g.kind];
                g.visible.clear();g.visible_fades.clear();
                if((g.kind!=WORLD_COVER_TREES||!tree_dispatch)&&(g.kind!=WORLD_COVER_GRASS||!grass_dispatch)) {
                    for (size_t i = 0; i < f.count; i++) {
                        const auto &a = f.instances[i];
                        if(g.kind==WORLD_COVER_GRASS&&(grass_transition.fades()[i]<=0.f||!grass.density_keep[i]))continue;
                        // Cached tree species is independent of camera distance.
                        // Reject other species before repeating the same distance
                        // work; tree selection never uses the non-tree root hash.
                        if(g.kind==WORLD_COVER_TREES&&tree_species[i]!=g.variant)continue;
                        double x = f.anchor_x + a.x - cam.x, z = f.anchor_z + a.z - cam.z;
                        double d2 = x*x + z*z;
                        if (d2 >= g.far_distance*g.far_distance || d2 < g.near_distance*g.near_distance) continue;
                        if(g.kind!=WORLD_COVER_TREES&&root_hash(f,a)%g.variants!=g.variant)continue;
                        if(g.kind==WORLD_COVER_GRASS) {
                            const double wx=f.anchor_x+a.x,wz=f.anchor_z+a.z;
                            const double radius=g.bound_radius*a.scale*1.4+.25,top=a.y+g.bound_top*a.scale*1.4+.15;
                            if(world_vegetation_sphere_outside(grass_view,wx,(a.y+top)*.5,wz,
                                std::hypot(radius,(top-a.y)*.5),70.*foliage_detail_scale)){scanned_culled_frustum++;continue;}
                            if(studio_castle_culling&&world_castle_wall_occludes(grass_wall,wx-village_x-radius,a.y-.05,wz-village_z-radius,
                                wx-village_x+radius,top,wz-village_z+radius)){scanned_culled_wall++;continue;}
                        }
                        // Near trees remain around the camera so off-screen branches
                        // can cast into view. Distant cards are frustum culled.
                        if (!g.shadow) {
                            double horizontal=x*forward.x+z*forward.z;
                            double depth = std::max(horizontal+(a.y+12*a.scale-cam.y)*forward.y,
                                                    horizontal-(a.y+12*a.scale+cam.y)*forward.y);
                            if(pool_mirror)depth=std::max(depth,horizontal-(a.y+12*a.scale+cam.y-2*pool->surface_y)*forward.y);
                            double across = x*right.x + z*right.z;
                            if (depth < -35 || std::fabs(across) > std::max(0.,depth)*cam.aspect*std::tan(double(cam.fov)*.5) + 40) continue;
                        }
                        g.selected.push_back({d2, i});
                    }
                }
                if(record_stats&&g.kind==WORLD_COVER_GRASS)peak_scanned_group_candidates=std::max(peak_scanned_group_candidates,g.selected.size());
                if (g.selected.size() > g.cap) {
                    if(record_stats&&g.kind==WORLD_COVER_GRASS)scanned_grass_cap_drops+=g.selected.size()-g.cap;
                    std::nth_element(g.selected.begin(), g.selected.begin()+g.cap, g.selected.end());
                    g.selected.resize(g.cap);
                    std::sort(g.selected.begin(),g.selected.end(),[](const auto&a,const auto&b){return a.second<b.second;});
                }
                // Without a cap overflow the source-index scan is already
                // sorted; avoid sorting the same stable roots on every turn.
                for (const auto &c : g.selected) {
                    auto instance=f.instances[c.second];
                    // Far-card meshes contain transparent canvas below the root;
                    // compensate the loader's geometry-base alignment.
                    if(g.kind==WORLD_COVER_TREES && g.lod==2)
                        instance.y-=(g.variant==0 ? .75f : (g.variant==1?.66f:0.f))*instance.scale;
                    if(g.kind==WORLD_COVER_GRASS) instance.scale*=1.4f;
                    if(g.kind==WORLD_COVER_ROCKS)
                        instance.y-=(g.variant==0?1.8f:(g.variant==1?2.5f:3.2f))*.20f*instance.scale;
                    g.visible.push_back(instance);
                    if(g.kind==WORLD_COVER_GRASS)g.visible_fades.push_back(grass_transition.fades()[c.second]);
                }
                if (g.asset && (g.uploaded.size() != g.visible.size() || (!g.visible.empty() &&
                    std::memcmp(g.uploaded.data(),g.visible.data(),g.visible.size()*sizeof(WorldModelInstance)) != 0)||g.visible_fades!=g.uploaded_fades)) {
                    bool ready=upload(g,g.visible,g.visible_fades);
                    if(ready){g.uploaded=g.visible;g.uploaded_fades=g.visible_fades;}
                    else upload(g,g.uploaded,g.uploaded_fades);
                }
                if(g.kind==WORLD_COVER_GRASS){scanned_grass_count+=g.visible.size();if(g.lod==1)scanned_mid_count+=g.visible.size();else scanned_near_count+=g.visible.size();}
                if (g.kind == WORLD_COVER_TREES) {
                    tree_count += g.visible.size();
                    if (g.near_distance == 0) near_tree_count += g.visible.size();
                    else if (g.near_distance < 100) mid_tree_count += g.visible.size();
                    else far_tree_count += g.visible.size();
                }
            }
            for(auto &v:reflection_instances) v.clear();
            for(const auto &g:groups) if(g.kind==WORLD_COVER_TREES) {
                auto &list=reflection_instances[g.variant];
                // Overlapping color LOD bands represent one world root. Use
                // their central interval when building reflection proxies.
                for(size_t i=0;i<g.visible.size()&&list.size()<4096;i++) {
                    auto a=g.visible[i];
                    double dx=fields[WORLD_COVER_TREES].anchor_x+a.x-cam.x,dz=fields[WORLD_COVER_TREES].anchor_z+a.z-cam.z;
                    double d=std::hypot(dx,dz);
                    if((g.lod==0&&d>=72)||(g.lod==1&&(d<72||d>=240))||(g.lod==2&&d<240)) continue;
                    if(g.lod!=2) a.y-=(g.variant==0?.75f:(g.variant==1?.66f:0.f))*a.scale;
                    list.push_back(a);
                }
            }
            for(unsigned i=0;i<4;i++) {
                auto &list=reflection_instances[i],&old=reflection_uploaded[i];
                if(old.size()!=list.size()||(!list.empty()&&std::memcmp(old.data(),list.data(),list.size()*sizeof(WorldModelInstance))!=0))
                    if(world_model_upload_instances(reflection_assets[i],list.data(),list.size()))old=list;
            }
        }
        size_t shadow_proxy_count() const {
            size_t count=0;for(const auto &g:groups)if(g.shadow_asset)count++;return count;
        }
        bool shadow_proxies_synced() const {
            for(const auto &g:groups)for(auto *asset:{g.shadow_asset,g.shadow_cull_asset})if(asset){
                if(world_model_instance_count(asset)!=g.shadow_uploaded.size()||g.shadow_uploaded.size()>g.uploaded.size())return false;
                for(const auto &a:g.shadow_uploaded)if(std::none_of(g.uploaded.begin(),g.uploaded.end(),[&](const auto &b){return !std::memcmp(&a,&b,sizeof a);}))return false;
            }
            return true;
        }
        void prepare_tree_shadows(const M4 &vp,const Camera &cam){
            if(!cover)return;
            if(!shadow_lod||!tree_shadow_culling)return;
            // Separate instance streams preserve every main/reflection root.
            // The production six-plane box test fails open and adds a4m
            // guard, enclosing the full rotated crown and bounded wind.
            for(auto &g:groups){
                auto *asset=g.shadow_asset?g.shadow_asset:g.shadow_cull_asset;
                if(g.kind!=WORLD_COVER_TREES||!g.shadow||!asset)continue;
                const auto &f=fields[g.kind];g.shadow_selected.clear();
                for(const auto &a:g.uploaded){
                    tree_shadow_candidates++;
                    if(world_terrain_box_visible(vp.m,f.anchor_x+a.x-cam.x,a.y+g.bound_top*a.scale*.5-cam.y,
                        f.anchor_z+a.z-cam.z,g.bound_radius*a.scale,g.bound_top*a.scale*.5,g.bound_radius*a.scale))g.shadow_selected.push_back(a);
                    else tree_shadow_rejected++;
                }
                if(g.shadow_selected.size()!=g.shadow_uploaded.size()||
                    (!g.shadow_selected.empty()&&std::memcmp(g.shadow_selected.data(),g.shadow_uploaded.data(),g.shadow_selected.size()*sizeof(WorldModelInstance)))){
                    if(world_model_upload_instances(asset,g.shadow_selected.data(),g.shadow_selected.size()))g.shadow_uploaded.assign(g.shadow_selected.begin(),g.shadow_selected.end());
                }
            }
        }
        void draw_reflected(Program &p,const Camera &cam) {
            if(!cover)return;
            auto &f=fields[WORLD_COVER_TREES];
            p.v3("uOffset",{float(f.anchor_x-cam.x),float(-cam.y),float(f.anchor_z-cam.z)});
            p.v2("uWorldAnchor",float(f.anchor_x),float(f.anchor_z));
            p.i("uGrass",0);p.i("uVegetation",4);p.i("uLodLevel",-1);p.f("uWind",.4f);
            for(unsigned i=0;i<4;i++) {p.f("uTreeHeight",reflection_heights[i]);world_model_draw(reflection_assets[i],p.id);}
        }
        bool prepare_probe_trees() {
            if(!cover)return true;
            // Capture all directions from the published field, independently
            // of the player's view. Only loading touches these temporary VBOs.
            std::array<std::vector<WorldModelInstance>,4> roots;
            for(auto &list:roots)list.reserve(4096);
            const auto &field=fields[WORLD_COVER_TREES];
            for(size_t i=0;i<field.count&&i<tree_species.size();i++) {
                const unsigned type=tree_species[i];auto &list=roots[type];if(list.size()==4096)continue;
                auto a=field.instances[i];a.y-=(type==0?.75f:(type==1?.66f:0.f))*a.scale;list.push_back(a);
            }
            bool ready=true;
            for(unsigned i=0;i<4;i++)ready=world_model_upload_instances(reflection_assets[i],roots[i].data(),roots[i].size())&&ready;
            return ready;
        }
        bool restore_probe_trees() {
            if(!cover)return true;
            bool ready=true;
            for(unsigned i=0;i<4;i++)ready=world_model_upload_instances(reflection_assets[i],reflection_uploaded[i].data(),reflection_uploaded[i].size())&&ready;
            return ready;
        }
        void draw(Program &p, const Camera &cam, bool shadows = false,Program *grass_program=nullptr,bool record_variant_stats=true,bool local_shadow_pass=false) {
            if(!cover)return;
            p.i("uGrass", 0);
            Program *active=&p;
            for (auto &g : groups) {
                if (shadows && (!g.shadow||(local_shadow_pass&&g.kind==WORLD_COVER_GRASS))) continue;
                Program &color=!shadows&&grass_program&&g.kind==WORLD_COVER_GRASS&&g.plain_grass_materials&&!g.visible.empty()?*grass_program:p;
                if(active!=&color){color.use();active=&color;}
                if(&color!=&p&&record_variant_stats){specialized_batches++;specialized_instances+=g.visible.size();}
                color.i("uGrass",0);
                const auto &f = fields[g.kind];
                color.v3("uOffset", {float(f.anchor_x-cam.x),float(-cam.y),float(f.anchor_z-cam.z)});
                color.v2("uWorldAnchor",float(f.anchor_x),float(f.anchor_z));
                color.i("uVegetation",g.vegetation);
                color.i("uLodLevel",g.lod);
                const bool grass_pair=g.kind==WORLD_COVER_GRASS&&g.lod>=0;
                color.v3("uPropLodRange",grass_pair?V3{18.f*foliage_detail_scale,4.f*foliage_detail_scale,g.lod==0?1.f:2.f}:V3{});
                color.f("uTreeHeight",g.tree_height);
                color.i("uRootedGrassWind",g.kind==WORLD_COVER_GRASS?1:0);
                color.f("uWind",g.kind==WORLD_COVER_ROCKS?0.f:g.kind==WORLD_COVER_GRASS?.45f:.75f);
                auto *asset=shadows&&shadow_lod?(g.shadow_asset?g.shadow_asset:(tree_shadow_culling&&g.shadow_cull_asset?g.shadow_cull_asset:g.asset)):g.asset;
                if(asset)world_model_draw(asset,color.id);
                if(!shadows)for(auto *extra:g.overflow_assets)if(extra)world_model_draw(extra,color.id);
            }
            active->v3("uPropLodRange",{});active->i("uRootedGrassWind",0);
            if(active!=&p)p.use();
            p.v3("uPropLodRange",{});p.i("uRootedGrassWind",0);
            if (!shadows&&!(std::getenv("VALWOOD_QA_SKIP_DRAW")&&std::strstr(std::getenv("VALWOOD_QA_SKIP_DRAW"),"blades"))) grass.draw(p,cam);
        }
        void release() {
            world_cover_shutdown(cover);
            for(auto &g:groups){world_model_free(g.asset);world_model_free(g.shadow_asset);world_model_free(g.shadow_cull_asset);for(auto *extra:g.overflow_assets)world_model_free(extra);}
            groups.clear();
            for(auto *a:reflection_assets) world_model_free(a);
            grass.release();
        }
    };

    // Distant woodland is a separate, coarse world-space field. It fills the
    // opposite slopes without spending near-tree geometry or rebuilding every
    // local 128m cover shift. CPU work and GPU batches have fixed upper bounds.
    struct HorizonForest {
        static constexpr size_t root_cap=100000,bucket_cap=3500;
        static constexpr double radius=10500.,step=60.;
        struct Bucket {
            WorldModelAsset *asset=nullptr;
            std::vector<WorldModelInstance> instances,uploaded;
            std::vector<std::pair<double,size_t>> selected;
            WorldSelectionMembership::Commit membership;
        };
        std::array<Bucket,8> buckets;
        WorldSelectionMembership membership_scratch;
        WorldHorizonGrid spatial_grid;
        uint64_t live_publication=0;
        uint64_t selections=0,reused_upload_batches=0,uploads=0,uploaded_instances=0;
        uint64_t grid_classifications=0,grid_cells=0,grid_retained_cells=0,grid_candidate_roots=0,grid_fallbacks=0;
        struct RankedRoot {uint64_t rank;WorldModelInstance instance;};
        std::vector<WorldModelInstance> live;
        std::vector<uint8_t> live_bucket;
        std::vector<RankedRoot> pending;
        double target_x=1e30,target_z=1e30,live_x=0,live_z=0;
        int64_t min_x=0,min_z=0,max_x=0,max_z=0,cx=0,cz=0;
        unsigned shoot=0;
        bool building=false;
        Camera selection_camera;
        bool has_selection=false;
        size_t drawn=0;
        static uint64_t hash(int64_t x,int64_t z,uint64_t salt) {
            uint64_t h=uint64_t(x)*0x9e3779b97f4a7c15ull^uint64_t(z)*0xbf58476d1ce4e5b9ull^salt*0x94d049bb133111ebull;
            h^=h>>30;h*=0xbf58476d1ce4e5b9ull;h^=h>>27;h*=0x94d049bb133111ebull;return h^(h>>31);
        }
        static double random(uint64_t h) {return double(h>>11)*(1./9007199254740992.);}
        bool init() {
            if(source_showcase)return true;
            if(!membership_scratch.initialize(root_cap)||!spatial_grid.initialize(root_cap))return false;
            live.reserve(root_cap);live_bucket.reserve(root_cap);pending.reserve(root_cap);
            bool ok=true;
            for(size_t i=0;i<buckets.size();i++) {
                auto &b=buckets[i];
                if(!b.membership.initialize(bucket_cap))return false;
                b.asset=world_model_load_budgeted(i<4 ? "models/cinematic_pine_far.glb" : "models/cinematic_fir_far.glb",environment_texture_edge);
                b.instances.reserve(bucket_cap);b.uploaded.reserve(bucket_cap);b.selected.reserve(root_cap/4);
                ok=b.asset&&ok;
            }
            return ok;
        }
        static void bound_selection(std::vector<std::pair<double,size_t>> &selection,size_t cap=bucket_cap) {
            if(selection.size()>cap) {
                std::nth_element(selection.begin(),selection.begin()+cap,selection.end());
                selection.resize(cap);
            }
        }
        void update(const Camera &cam,size_t budget=2048) {
            if(source_showcase)return;
            bool published=false;
            double ax=std::floor(cam.x/1024.)*1024,az=std::floor(cam.z/1024.)*1024;
            if(ax!=target_x||az!=target_z) {
                target_x=ax;target_z=az;pending.clear();shoot=0;building=true;
                min_x=int64_t(std::floor((ax-radius)/step));min_z=int64_t(std::floor((az-radius)/step));
                max_x=int64_t(std::ceil((ax+radius)/step));max_z=int64_t(std::ceil((az+radius)/step));cx=min_x;cz=min_z;
            }
            for(size_t i=0;i<budget&&building;i++) {
                // Two stable roots per coarse cell produce clustered distant
                // canopy, with photographic silhouettes rather than flat blobs.
                uint64_t h=hash(cx,cz,1901+shoot*17);
                double px=(cx+.12+random(h)*.76)*step,pz=(cz+.12+random(hash(cx,cz,1903+shoot*17))*.76)*step;
                if(std::hypot(px-target_x,pz-target_z)<=radius) {
                    auto a=world_atlas_sample(px,pz);
                    auto authored=world_atlas_landscape_mask(px,pz);
                    double treeline=1.-std::clamp((double(a.height)-1000.)/450.,0.,1.);
                    double rock_suitability=1.-std::clamp((double(authored.rock)-.18)/.57,0.,1.);
                    double probability=a.forest_density*.95*treeline*rock_suitability;
                    if(source_showcase||(stronghold_finish&&std::hypot(px-stronghold_x,pz-stronghold_z)<350))probability=0;
                    if(studio_document){const int area=world_ecology::at_layer(studio_document->ecology.data(),studio_document->ecology.size(),px,pz,WORLD_COVER_TREES);if(area>=0)probability=world_ecology::density(studio_document->ecology[unsigned(area)],WORLD_COVER_TREES,px,pz);if(!studio_document->stamps.empty())probability*=world_studio::vegetation(*studio_document,1,px,pz);}
                    if(a.height>3&&a.height<1450&&a.shore_distance>35&&a.land_weight>.85&&!world_atlas_local_water(px,pz,nullptr,nullptr)&&random(hash(cx,cz,1907+shoot*17))<probability) {
                        float scale=float(1.0+random(hash(cx,cz,1909+shoot*17))*.65);
                        WorldModelInstance instance={float(px-target_x),a.height,float(pz-target_z),float(random(hash(cx,cz,1913+shoot*17))*6.2831853),scale};
                        // Keep nearby horizon roots, then select overflow by a
                        // stable world rank. A full field cannot favor the first
                        // north-to-south scan rows or replace all nearby roots.
                        uint64_t tier=std::hypot(px-target_x,pz-target_z)<3200.?0ull:(1ull<<63);
                        RankedRoot root={tier|(hash(cx,cz,1999+shoot*17)>>1),instance};
                        auto compare=[](const RankedRoot&a,const RankedRoot&b){return a.rank<b.rank;};
                        if(pending.size()<root_cap) {pending.push_back(root);std::push_heap(pending.begin(),pending.end(),compare);}
                        else if(root.rank<pending.front().rank) {
                            std::pop_heap(pending.begin(),pending.end(),compare);pending.back()=root;
                            std::push_heap(pending.begin(),pending.end(),compare);
                        }
                    }
                }
                if(++shoot==2) {shoot=0;if(++cx>max_x) {cx=min_x;++cz;}}
                if(cz>max_z) {
                    live.clear();live_bucket.clear();
                    for(const auto &root:pending) {
                        live.push_back(root.instance);
                        const auto &a=live.back();
                        // Match the old selection hash after local-float
                        // quantization, using the newly published anchor.
                        uint64_t h=hash(int64_t(std::floor((target_x+a.x)*4)),
                                        int64_t(std::floor((target_z+a.z)*4)),1973);
                        live_bucket.push_back(static_cast<uint8_t>(h%8));
                    }
                    pending.clear();live_x=target_x;live_z=target_z;building=false;
                    spatial_grid.publish(live.data(),live.size());
                    published=true;++live_publication;
                    for(auto &b:buckets)b.membership.invalidate();
                }
            }
            const float selection_turn_scale=std::max(1.f,cam.aspect/(16.f/9.f));
            // Horizon cards have a55m guard; a3m translation or .002rad
            // rotation stays inside it over the10.5km field. Generation above
            // continues every frame even while the visible selection is reused.
            if(!published&&has_selection&&cam.aspect==selection_camera.aspect&&
               std::hypot(cam.x-selection_camera.x,cam.z-selection_camera.z)<3.&&
               std::fabs(cam.y-selection_camera.y)<.5&&
               std::fabs(std::remainder(cam.yaw-selection_camera.yaw,6.2831853f))<.002f/selection_turn_scale&&
               std::fabs(cam.pitch-selection_camera.pitch)<.002f/selection_turn_scale) return;
            selection_camera=cam;has_selection=true;selections++;
            for(auto &b:buckets)b.selected.clear();
            V3 forward=cam.forward(),right=cam.right();
            const auto *pool=world_atlas_rainforest_pool();
            const bool pool_mirror=std::hypot(cam.x-pool->x,cam.z-pool->z)<220.;
            // Horizon grid broad phase begin: only proven whole-cell misses.
            WorldHorizonGrid::View grid_view;
            grid_view.anchor_x=live_x;grid_view.anchor_z=live_z;
            grid_view.x=cam.x;grid_view.y=cam.y;grid_view.z=cam.z;
            grid_view.forward_x=forward.x;grid_view.forward_y=forward.y;grid_view.forward_z=forward.z;
            grid_view.right_x=right.x;grid_view.right_z=right.z;
            grid_view.aspect=cam.aspect;grid_view.pool_mirror=pool_mirror;grid_view.pool_y=pool->surface_y;
            const auto grid_stats=spatial_grid.classify(grid_view);grid_classifications++;
            grid_cells+=grid_stats.cells;grid_retained_cells+=grid_stats.retained_cells;
            grid_candidate_roots+=grid_stats.accelerated?grid_stats.candidate_roots:live.size();
            grid_fallbacks+=!grid_stats.accelerated;
            // Horizon grid broad phase end. Keep the original point arithmetic
            // and input order, including capped nearest-root tie behavior.
            for(size_t i=0;i<live.size();i++) {
                if(!spatial_grid.includes(i))continue;
                const auto &a=live[i];
                double dx=live_x+a.x-cam.x,dz=live_z+a.z-cam.z,d2=dx*dx+dz*dz;
                if(d2<1450.*1450.||d2>12000.*12000.) continue;
                double horizontal=dx*forward.x+dz*forward.z;
                double depth=std::max(horizontal+(a.y+17*a.scale-cam.y)*forward.y,
                                      horizontal-(a.y+17*a.scale+cam.y)*forward.y);
                if(pool_mirror)depth=std::max(depth,horizontal-(a.y+17*a.scale+cam.y-2*pool->surface_y)*forward.y);
                if(depth < -45 || std::fabs(dx*right.x+dz*right.z)>std::max(0.,depth)*cam.aspect*std::tan(double(cam.fov)*.5)+55) continue;
                buckets[live_bucket[i]].selected.push_back({d2,i});
            }
            drawn=0;
            for(size_t index=0;index<buckets.size();index++) {
                auto &b=buckets[index];
                bound_selection(b.selected);
                if(membership_scratch.equal(b.membership,b.selected,live_publication)) {
                    // Retain the exact committed root order and GL buffer when
                    // nearest-distance partitioning merely permutes that set.
                    drawn+=b.membership.gpu_count();reused_upload_batches++;continue;
                }
                b.instances.clear();
                // Membership is selected by distance, while cutout roots keep
                // world-seeded attributes independent of buffer order. Avoid a
                // redundant O(n log n) sort of thousands of horizon cards.
                for(const auto &item:b.selected) {
                    auto a=live[item.second];a.y-=(index<4?.75f:.66f)*a.scale;b.instances.push_back(a);
                }
                if(world_model_upload_instances(b.asset,b.instances.data(),b.instances.size())) {
                    uploads++;uploaded_instances+=b.instances.size();
                    if(membership_scratch.commit_if_uploaded(b.membership,b.selected,live_publication,true,live_x,live_z)) {
                        b.uploaded=b.instances;
                    }else {
                        // The GPU accepted this payload even if metadata could
                        // not commit: retain its actual origin until recovery.
                        b.membership.note_probe_upload(live_x,live_z,b.instances.size(),true);
                        b.membership.invalidate();has_selection=false;
                    }
                }else {
                    // The old committed buffer/set remain authoritative. Retry
                    // next frame rather than waiting for another3m movement.
                    has_selection=false;
                }
                drawn+=b.membership.gpu_count();
            }
        }
        void draw(Program &p,const Camera &cam) {
            if(source_showcase)return;
            p.i("uGrass",0);p.i("uVegetation",4);p.i("uLodLevel",3);p.f("uWind",.4f);
            bool anchored=false;double bound_x=0,bound_z=0;
            for(size_t i=0;i<buckets.size();i++) {
                const auto &b=buckets[i];
                if(b.membership.gpu_count()) {
                    const double x=b.membership.gpu_anchor_x(),z=b.membership.gpu_anchor_z();
                    if(!anchored||x!=bound_x||z!=bound_z) {
                        p.v3("uOffset",{float(x-cam.x),float(-cam.y),float(z-cam.z)});
                        p.v2("uWorldAnchor",float(x),float(z));bound_x=x;bound_z=z;anchored=true;
                    }
                }
                p.f("uTreeHeight",i<4?25.f:22.f);world_model_draw(b.asset,p.id);
            }
        }
        bool prepare_probe_trees(const Camera &cam) {
            if(source_showcase)return true;
            for(auto &b:buckets){b.selected.clear();b.instances.clear();}
            for(size_t i=0;i<live.size();i++) {
                const auto &a=live[i];const double dx=live_x+a.x-cam.x,dz=live_z+a.z-cam.z,d2=dx*dx+dz*dz;
                if(d2>=1450.*1450.&&d2<=12000.*12000.)buckets[live_bucket[i]].selected.push_back({d2,i});
            }
            bool ready=true;
            for(size_t i=0;i<buckets.size();i++) {
                auto &b=buckets[i];bound_selection(b.selected);
                for(const auto &root:b.selected){auto a=live[root.second];a.y-=(i<4?.75f:.66f)*a.scale;b.instances.push_back(a);}
                const bool uploaded=world_model_upload_instances(b.asset,b.instances.data(),b.instances.size());
                if(uploaded){uploads++;uploaded_instances+=b.instances.size();}
                b.membership.note_probe_upload(live_x,live_z,b.instances.size(),uploaded);
                ready=uploaded&&ready;
            }
            return ready;
        }
        bool restore_probe_trees() {
            if(source_showcase)return true;
            bool ready=true;
            for(auto &b:buckets){
                const bool uploaded=world_model_upload_instances(b.asset,b.uploaded.data(),b.uploaded.size());
                if(uploaded){uploads++;uploaded_instances+=b.uploaded.size();}
                else {b.membership.invalidate();has_selection=false;}
                b.membership.note_restore_upload(uploaded);
                ready=uploaded&&ready;
            }
            return ready;
        }
        void release() {for(auto &b:buckets)world_model_free(b.asset);}
    };

    struct CoastalOutcrops {
        WorldModelAsset *asset=nullptr;
        std::vector<WorldModelInstance> instances;
        double anchor_x=1e30,anchor_z=1e30;
        bool init() {
            if(source_showcase)return true;
            asset=world_model_load_budgeted("models/cinematic_cliff_01.glb",environment_texture_edge);
            instances.reserve(64);
            return asset!=nullptr;
        }
        void update(const Camera &cam) {
            if(source_showcase)return;
            double ax=std::floor(cam.x/256.)*256,az=std::floor(cam.z/256.)*256;
            if(ax==anchor_x&&az==anchor_z) return;
            anchor_x=ax;anchor_z=az;instances.clear();
            int64_t start_x=int64_t(std::floor((ax-1350)/300.)),start_z=int64_t(std::floor((az-1350)/300.));
            for(int z=0;z<11;z++) for(int x=0;x<11;x++) {
                double px=(start_x+x+.37)*300,pz=(start_z+z+.61)*300;
                auto a=world_atlas_sample(px,pz);
                if(a.shore_distance<35||a.shore_distance>340||a.height<1||a.height>85||
                   (a.region!=WORLD_ATLAS_CROSSWIND&&a.region!=WORLD_ATLAS_KERONE)) continue;
                double gx=world_atlas_sample(px+24,pz).shore_distance-world_atlas_sample(px-24,pz).shore_distance;
                double gz=world_atlas_sample(px,pz+24).shore_distance-world_atlas_sample(px,pz-24).shore_distance;
                double len=std::hypot(gx,gz);
                if(len<1) continue;
                gx/=len;gz/=len;
                px-=gx*(a.shore_distance-42);pz-=gz*(a.shore_distance-42);
                a=world_atlas_sample(px,pz);
                if(a.height<1||a.height>25||a.shore_distance<12||a.shore_distance>100) continue;
                uint32_t hash=uint32_t(start_x+x)*0x9e3779b9u^uint32_t(start_z+z)*0x85ebca6bu;
                float scale=.75f+float(hash%1000)*.0007f;
                bool overlap=false;
                for(const auto &old:instances)
                    if(std::hypot(px-anchor_x-old.x,pz-anchor_z-old.z)<70) {overlap=true;break;}
                for(const auto &view:scenic_views) {
                    double vx,vz;world_atlas_uv_to_world(view.u,view.v,&vx,&vz);
                    if(std::hypot(px-vx,pz-vz)<60) overlap=true;
                }
                if(overlap||instances.size()==64) continue;
                instances.push_back({float(px-anchor_x),a.height-6.f*scale,float(pz-anchor_z),float(std::atan2(-gx,-gz)),scale});
            }
            world_model_upload_instances(asset,instances.data(),instances.size());
        }
        void draw(Program &p,const Camera &cam) {
            if(source_showcase)return;
            p.v3("uOffset",{float(anchor_x-cam.x),float(-cam.y),float(anchor_z-cam.z)});
            p.v2("uWorldAnchor",float(anchor_x),float(anchor_z));
            p.i("uGrass",0);p.i("uVegetation",0);p.i("uLodLevel",-1);p.f("uWind",0);
            world_model_draw(asset,p.id);
        }
        void release() {world_model_free(asset);asset=nullptr;}
    };

    int world_panel(Camera &cam, TerrainGPU &terrain, GLuint &map, bool &map_attempted, bool tools_visible, bool & show_map, bool &show_graphics, float &hour, float &cloud, float &rain, float &wind, float &mist,
        float &bearing,float &sun_strength,float &sky_fill,float &exposure,float &grade_strength,float fps,
        bool & running, const char *current, int &scenic_destination,const WorldVillagePopulation *population) {
        int travel = -1;
        size_t count;
        auto landmarks = world_atlas_landmarks(&count);
        if(tools_visible) {
        ImGui::SetNextWindowPos({18, 18}, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize({330, 0}, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSizeConstraints({300,180},{420,std::max(180.f,ImGui::GetIO().DisplaySize.y-36.f)});
        ImGui::SetNextWindowBgAlpha(.9f);
        ImGui::Begin("Valwood | World terrain");
        ImGui::TextColored({.91f, .76f, .46f, 1}, "%s", current);
        auto s = world_atlas_sample(cam.x, cam.z);
        ImGui::Text("%s", world_atlas_region_name(s.region));
        ImGui::Text("%.1f FPS  |  %s", fps, cam.fly ? "FLIGHT PREVIEW" : "ON FOOT");
        ImGui::Text("East %.2f km   South %.2f km", cam.x / 1000., cam.z / 1000.);
        ImGui::Text("Elevation %.1f m", world_atlas_height(cam.x, cam.z));
        if(world_atlas_village_weight(cam.x,cam.z)>.99f) {
            double ox,oz;world_atlas_village_origin(&ox,&oz);
            const double x=cam.x-ox,z=cam.z-oz;
            const char *district=z<-55&&std::abs(x)<65?"Castle ward":
                (std::abs(x)<40&&z>-30&&z<43?"Traders square":
                 (z>175?"South gate":(x<0?"West residential quarter":"East residential quarter")));
            ImGui::TextColored({.85f,.77f,.56f,1},"Valdoa | %s",district);
            auto people=world_village_population_stats(population);
            ImGui::Text("%zu residents | %zu on the move",people.residents,people.walking);
            WorldVillageResident nearest;
            if(world_village_population_closest(population,cam.x,cam.z,8,&nearest)) {
                ImGui::TextWrapped("Nearby: %s | %s",nearest.name,nearest.role);
            }
        }
        ImGui::Separator();
        ImGui::Text("100.8 x 56.7 km");
        ImGui::TextWrapped("8 hours across the reference width at 3.5 m/s. Terrain, water and detours change route times.");
        ImGui::Separator();
        ImGui::TextWrapped("WASD walk  |  RMB look  |  Shift sprint  |  V camera\nF fly / walk  |  Q / E flight height\nTab map  |  1-9 landmarks  |  H tools\nF9 showcase tour  |  F10 graphics\nJ journal  |  F5 save  |  F8 load");
        if(ImGui::Button("Graphics and performance [F10]"))show_graphics=true;
        if (ImGui::Button(show_map ? "Close atlas [Tab]" : "Open atlas [Tab]"))
            show_map = !show_map;
        if (ImGui::Button(cam.fly ? "Return to walking [F]" : "Fly over the terrain [F]")) {
            cam.fly = !cam.fly;
            cam.y = std::max(world_atlas_height(cam.x, cam.z), 0.f) + (cam.fly ? 150. : 1.75);
            cam.pitch = cam.fly ? -.35f : -.04f;
        }

        if (ImGui::CollapsingHeader("Light and atmosphere")) {
            ImGui::SliderFloat("Hour", &hour, 5.5f, 20.f, "%.1f");
            ImGui::SliderFloat("Sun bearing", &bearing, -180.f, 180.f, "%.0f deg");
            ImGui::SliderFloat("Sun strength", &sun_strength, .25f, 2.f);
            ImGui::SliderFloat("Sky fill", &sky_fill, .25f, 2.f);
            ImGui::SliderFloat("Exposure", &exposure, .5f, 1.75f);
            ImGui::SliderFloat("Village color grade", &grade_strength, 0.f, 1.f);
            ImGui::SliderFloat("Cloud", &cloud, 0, 1);
            ImGui::SliderFloat("Rain", &rain, 0, 1);
            ImGui::SliderFloat("Wind", &wind, 0, 1.5f);
            ImGui::SliderFloat("Mist", &mist, 0, 1.5f);
            ImGui::TextWrapped("Rain follows the rainforest. Mist adds a softer ground layer there and light atmospheric depth in Valdoa.");
        }

        if (ImGui::CollapsingHeader("Scenic trails", ImGuiTreeNodeFlags_DefaultOpen))
            for (size_t i=0; i<sizeof(scenic_views)/sizeof(scenic_views[0]); i++)
                if (ImGui::Button(scenic_views[i].name, {-1,0})) scenic_destination=int(i);
        if (ImGui::CollapsingHeader("Landmark travel"))
            for (size_t i = 0; i < count; i++)
                if (ImGui::Button(landmarks[i].name, {-1, 0}))
                    travel = (int) i;


        WorldTilesMetrics m = {};
        world_tiles_metrics(terrain.stream, &m);
        if (ImGui::CollapsingHeader("Streaming budget")) {
            ImGui::Text("%zu / 256 terrain records", m.desired);
            ImGui::Text("%zu ready draws / %zu queued", terrain.draws.size(), m.queued + m.building);
            ImGui::Text("%.1f MiB terrain GPU", terrain.bytes() / 1048576.);
            ImGui::Text("%.1f MiB terrain CPU peak", m.peak_cpu_bytes / 1048576.);
        }

        if (ImGui::Button("Exit world"))
            running = false;
        ImGui::End();
        }
        if (show_map) {
            // The full-resolution atlas belongs to this optional UI. Load it
            // once on first opening; ordinary play never allocates its72MiB.
            if(!map_attempted) {
                map_attempted=true;map=texture("assets/map.jpg",false);
                if(map) {
                    glBindTexture(GL_TEXTURE_2D,map);
                    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
                    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
                } else std::fprintf(stderr,"[World] Atlas image failed to load: assets/map.jpg\n");
            }
            ImGui::SetNextWindowSize({1000, 650}, ImGuiCond_FirstUseEver);
            ImGui::Begin("The lands of Valwood", &show_map);
            if(!map) {
                ImGui::TextWrapped("The atlas image could not be loaded. Landmark travel is available in the tools panel.");
                ImGui::End();return travel;
            }
            ImVec2 avail = ImGui::GetContentRegionAvail();
            float w = std::max(200.f, avail.x), h = w * 2818.f / 5009.f;
            if (h > avail.y - 48) {
                h = avail.y - 48;
                w = h * 5009.f / 2818.f;
            }
            ImVec2 p = ImGui::GetCursorScreenPos();
            ImGui::Image((ImTextureID)(intptr_t) map, {w, h});
            auto draw = ImGui::GetWindowDrawList();
            for (size_t i = 0; i < count; i++) {
                ImVec2 pt = {p.x + float(landmarks[i].u) * w, p.y + float(landmarks[i].v) * h};
                draw->AddCircleFilled(pt, 4, IM_COL32(248, 218, 132, 255));
                if (ImGui::IsItemHovered() && std::hypot(ImGui::GetIO().MousePos.x - pt.x, ImGui::GetIO().MousePos.y - pt.y) < 14) {
                    ImGui::SetTooltip("%s - click to travel", landmarks[i].name);
                    if (ImGui::IsMouseClicked(0))
                        travel = (int) i;
                }
            }
            double u, v;
            world_atlas_world_to_uv(cam.x, cam.z, &u, &v);
            draw->AddCircleFilled({p.x + float(u) * w, p.y + float(v) * h}, 6, IM_COL32(100, 220, 240, 255));
            draw->AddCircle({p.x + float(u) * w, p.y + float(v) * h}, 9, IM_COL32_WHITE, 20, 2);
            ImGui::TextWrapped("Gold markers: review destinations. Cyan: your position. Landmark travel is a preview tool; it does not change walking speed.");
            ImGui::End();
        }

        return travel;
    }

}

namespace {
void draw_combat_feedback(const world_combat_feedback::System &feedback){
    using Kind=world_combat_feedback::Kind;
    const auto &io=ImGui::GetIO();const float w=io.DisplaySize.x,h=io.DisplaySize.y;
    const float u=std::clamp(std::min(w/1600.f,h/900.f),.65f,1.4f);
    auto *draw=ImGui::GetForegroundDrawList();
    auto notice=[&](const world_combat_feedback::Cue &cue,float offset,unsigned row){
        if(cue.kind==Kind::None||cue.remaining<=0)return;
        const float fade=std::clamp(cue.remaining/.12f,0.f,1.f),cx=w*.5f,cy=h*.5f+offset;
        const ImU32 color=IM_COL32(cue.color[0],cue.color[1],cue.color[2],int(255*fade));
        const float r=12*u,thickness=2.4f*u;
        if(cue.kind==Kind::Body||cue.kind==Kind::Incoming){
            for(float x:{-1.f,1.f})for(float y:{-1.f,1.f})draw->AddLine({cx+x*r,cy+y*r},{cx+x*(r+7*u),cy+y*(r+7*u)},color,thickness);
        }else if(cue.kind==Kind::Miss){
            // Open neutral marks deliberately avoid the impact cross/flash.
            for(float side:{-1.f,1.f})draw->AddLine({cx+side*21*u,cy},{cx+side*29*u,cy},color,1.5f*u);
        }else if(cue.kind==Kind::Blocked){
            for(float side:{-1.f,1.f}){const float x=cx+side*18*u;draw->AddLine({x,cy-12*u},{x,cy+12*u},color,thickness);draw->AddLine({x,cy-12*u},{x-side*6*u,cy-12*u},color,thickness);draw->AddLine({x,cy+12*u},{x-side*6*u,cy+12*u},color,thickness);}
        }else if(cue.kind==Kind::Block){
            const ImVec2 shield[]={{cx-12*u,cy-12*u},{cx+12*u,cy-12*u},{cx+10*u,cy+4*u},{cx,cy+15*u},{cx-10*u,cy+4*u}};
            draw->AddPolyline(shield,5,color,ImDrawFlags_Closed,thickness);
        }else{
            const ImVec2 diamond[]={{cx,cy-18*u},{cx+18*u,cy},{cx,cy+18*u},{cx-18*u,cy}};
            draw->AddPolyline(diamond,4,color,ImDrawFlags_Closed,thickness);draw->AddCircleFilled({cx,cy},2.5f*u,color);
        }
        std::string caption=cue.caption;
        if((cue.kind==Kind::Body||cue.kind==Kind::Incoming)&&cue.damage>0)caption+="  -"+std::to_string(int(std::lround(cue.damage)));
        const float font=cue.kind==Kind::Miss?15*u:20*u;
        const float width=ImGui::CalcTextSize(caption.c_str()).x*font/ImGui::GetFontSize();
        const bool hint=cue.hint&&*cue.hint;
        const float hint_width=hint?ImGui::CalcTextSize(cue.hint).x*(12*u)/ImGui::GetFontSize():0;
        const float plate=std::max(width,hint_width)+24*u;
        const auto layout=world_combat_notice_layout(w,h,u,plate,row);const float y=layout.caption_y;
        draw->AddRectFilled({cx-plate*.5f,y-5*u},{cx+plate*.5f,y+(hint?42:25)*u},IM_COL32(8,14,19,int(190*fade)),4*u);
        draw->AddText(ImGui::GetFont(),font,{cx-width*.5f,y},color,caption.c_str());
        if(hint)draw->AddText(ImGui::GetFont(),12*u,{cx-hint_width*.5f,y+25*u},IM_COL32(229,233,229,int(235*fade)),cue.hint);
    };
    notice(feedback.attack_notice(),0,0);
    const bool attack_visible=feedback.attack_notice().remaining>0;
    notice(feedback.defense_notice(),attack_visible?94*u:0,attack_visible?1:0);
}
struct TrainingSounds {
    static constexpr int voices=8;
    std::array<Mix_Chunk*,4> clips={};int first=-1;unsigned voice=0,events=0,body_events=0,metal_events=0;bool ready=false;
    bool init(){
        if(!Mix_QuerySpec(nullptr,nullptr,nullptr))return false;
        char *base=SDL_GetBasePath();if(!base)return false;
        const char *names[]={"snd_attack","snd_hit","snd_shield","snd_dash"};
        for(unsigned i=0;i<4;i++){char path[1400];if(std::getenv("VALWOOD_PRESENTATION")){const char *preview_names[]={"slash","impact","parry","dash"};std::snprintf(path,sizeof path,"audio/presentation/%s.wav",preview_names[i]);}else std::snprintf(path,sizeof path,"%ssound_effects/%s.wav",base,names[i]);clips[i]=Mix_LoadWAV(path);}
        SDL_free(base);for(auto *c:clips)if(!c){release();return false;}
        int hz=0,channels=0;Uint16 format=0;Mix_QuerySpec(&hz,&format,&channels);
        // Bring the owned swoosh's peak forward to the shorter windup. Keep
        // the source WAV unchanged and trim whole frames in our owned buffer.
        if(std::getenv("VALWOOD_PRESENTATION")&&hz>0&&channels>0){
            const Uint32 stride=Uint32(channels*(SDL_AUDIO_BITSIZE(format)/8));
            const Uint32 skip=Uint32(hz/20)*stride;
            if(stride&&clips[0]->alen>skip+stride){SDL_memmove(clips[0]->abuf,clips[0]->abuf+skip,clips[0]->alen-skip);clips[0]->alen-=skip;}
        }
        if(!std::getenv("VALWOOD_PRESENTATION")&&hz>0&&channels>0)clips[2]->alen=std::min(clips[2]->alen,Uint32(hz*channels*(SDL_AUDIO_BITSIZE(format)/8)/4));
        first=Mix_AllocateChannels(-1);
        if(first<0||first>128||Mix_AllocateChannels(first+voices)!=first+voices){first=-1;release();return false;}
        ready=true;return true;
    }
    bool play(unsigned clip,int volume,float pan=0,float gain=1,unsigned fade_ms=0){
        if(!ready||clip>=clips.size())return false;
        // A new contact must not erase the previous clang merely because both
        // use the same source clip. Reuse a bounded pool, never allocate here.
        int channel=-1;
        for(int n=0;n<voices;++n){const int candidate=first+int((voice+unsigned(n))%voices);if(!Mix_Playing(candidate)){channel=candidate;break;}}
        if(channel<0){channel=first+int(voice%voices);Mix_HaltChannel(channel);}
        voice=unsigned(channel-first+1)%voices;
        Mix_Volume(channel,int(std::clamp(float(volume)*gain,0.f,128.f)));
        pan=std::clamp(pan,-1.f,1.f);
        Mix_SetPanning(channel,Uint8(std::lround(255*(pan>0?1-pan:1))),Uint8(std::lround(255*(pan<0?1+pan:1))));
        const int played=clip==0?Mix_FadeInChannel(channel,clips[clip],0,4):Mix_PlayChannel(channel,clips[clip],0);
        if(played>=0){if(fade_ms)Mix_FadeOutChannel(played,int(fade_ms));events++;body_events+=clip==1;metal_events+=clip==2;}
        return played>=0;
    }
    void mute(){if(ready){for(int i=0;i<voices;++i)Mix_HaltChannel(first+i);}}
    void release(){
        if(first>=0){for(int i=0;i<voices;++i)Mix_HaltChannel(first+i);if(Mix_AllocateChannels(-1)==first+voices)Mix_AllocateChannels(first);first=-1;}
        for(auto *&c:clips){if(c)Mix_FreeChunk(c);c=nullptr;}ready=false;
    }
};
}

extern "C" int world_viewer_run(EngineLaunch *launch, EngineProject *project) {
    landscape_authoring_session=launch->editor;
    const bool authored_shadow_clip_enabled=std::getenv("VALWOOD_QA_DISABLE_AUTHORED_SHADOW_CLIP")==nullptr;
    generated_cover_collision={};generated_cover_collision_ready=true;generated_cover_kind_ready={true,true};
    const bool qa_landscape=launch->frames&&!launch->benchmark&&std::getenv("VALWOOD_QA_EDITOR_LANDSCAPE");
    unsigned qa_landscape_phase=0,qa_landscape_checks=0;bool qa_landscape_passed=false;
    const char *zoom_qa_document=std::getenv("VALWOOD_QA_STUDIO_DOCUMENT");
    const bool qa_editor_zoom=launch->editor&&launch->frames&&!launch->benchmark&&std::getenv("VALWOOD_QA_EDITOR_ZOOM")&&zoom_qa_document&&std::strncmp(zoom_qa_document,"/tmp/",5)==0;
    unsigned qa_zoom_phase=0,qa_zoom_checks=0,editor_dolly_events=0,editor_speed_events=0;bool qa_zoom_passed=false;
    const bool qa_item_wind=launch->editor&&launch->frames&&!launch->benchmark&&std::getenv("VALWOOD_QA_ITEM_WIND");
    unsigned qa_item_wind_checks=0,qa_item_wind_phase=0;bool qa_item_wind_passed=false;
    const bool qa_structure=launch->editor&&launch->frames&&!launch->benchmark&&std::getenv("VALWOOD_QA_STRUCTURE");
    unsigned qa_structure_checks=0,qa_structure_phase=0;bool qa_structure_passed=false;
    struct StructureWheelProbe {
        unsigned events=0,buttons=0;int raw_x=0,raw_y=0,x=0,y=0;
        bool eligible=false,capture=false,text=false,active=false,focus=false,app_focus=false,inside=false;
    } structure_wheel_probe;
    const bool qa_editor_npc=launch->editor&&launch->frames&&!launch->benchmark&&std::getenv("VALWOOD_QA_EDITOR_NPC");
    unsigned qa_npc_checks=0,qa_npc_phase=0;bool qa_npc_passed=false;
    const bool qa_structure_splines=launch->editor&&launch->frames&&!launch->benchmark&&std::getenv("VALWOOD_QA_STRUCTURE_SPLINES");
    const bool qa_editor_long_session=launch->editor&&launch->frames&&!launch->benchmark&&std::getenv("VALWOOD_QA_EDITOR_LONG_SESSION");
    const bool qa_editor_reliability=launch->editor&&launch->frames&&!launch->benchmark&&std::getenv("VALWOOD_QA_EDITOR_RELIABILITY");
    const bool qa_editor_move=launch->editor&&launch->frames&&!launch->benchmark&&std::getenv("VALWOOD_QA_EDITOR_MOVE")&&zoom_qa_document&&std::strncmp(zoom_qa_document,"/tmp/",5)==0;
    unsigned qa_editor_move_phase=0,qa_editor_move_key=0,qa_editor_move_start=0,qa_editor_move_applied=0;
    world_studio::Pose qa_editor_move_target;
    unsigned qa_reliability_phase=0,qa_reliability_checks=0;bool qa_reliability_passed=false;
    const bool qa_landscape_refine=qa_landscape&&launch->editor&&zoom_qa_document&&std::strncmp(zoom_qa_document,"/tmp/",5)==0&&std::getenv("VALWOOD_QA_EDITOR_LANDSCAPE_REFINE");
    unsigned qa_refine_phase=0,qa_refine_checks=0;bool qa_refine_passed=false;
    const bool qa_water_refine=launch->editor&&launch->frames&&!launch->benchmark&&zoom_qa_document&&std::strncmp(zoom_qa_document,"/tmp/",5)==0&&std::getenv("VALWOOD_QA_WATER_REFINE");
    unsigned qa_water_phase=0,qa_water_checks=0;bool qa_water_passed=false;double qa_water_joined_level=0;
    const bool presentation=std::getenv("VALWOOD_PRESENTATION")!=nullptr&&!launch->editor;
    const bool opening_enabled=presentation&&std::getenv("VALWOOD_OPENING_PROLOGUE")&&std::filesystem::is_regular_file("opening.config");
    const bool first_journey_enabled=opening_enabled&&std::filesystem::is_regular_file("opening/first-journey.settings");
    const bool battlefield_enabled=first_journey_enabled&&std::filesystem::is_regular_file("opening/battlefield.settings");
    const bool first_journey_qa=first_journey_enabled&&launch->frames&&!launch->benchmark&&std::getenv("VALWOOD_QA_FIRST_JOURNEY");
    const bool road_traffic_qa=opening_enabled&&launch->frames&&!launch->benchmark&&std::getenv("VALWOOD_QA_ROAD_TRAFFIC");
    if(road_traffic_qa){std::ifstream marker(".valwood-road-qa");std::string token,extra;if(launch->frames>1200||!(marker>>token)||token!="VALWOOD_PRIVATE_ROAD_V1"||(marker>>extra)){std::fprintf(stderr,"[Road QA] Marked bounded private project required\n");return 1;}}
    const bool npc_intro_qa=opening_enabled&&launch->frames&&!launch->benchmark&&std::getenv("VALWOOD_QA_NPC_INTRO");
    if(npc_intro_qa){
        std::ifstream marker(".valwood-npc-intro-qa");std::string token;
        if(!(marker>>token)||token!="VALWOOD_PRIVATE_NPC_INTRO_V1"){std::fprintf(stderr,"[NPC intro QA] Private fixture marker missing.\n");return 1;}
        marker>>std::ws;if(!marker.eof())return 1;
    }
    const bool opening_qa=opening_enabled&&launch->frames&&!launch->benchmark&&(std::getenv("VALWOOD_QA_OPENING_PROLOGUE")||first_journey_qa);
    const bool opening_house_geometry=std::filesystem::is_regular_file("opening/home.settings");
    world_opening_interior::Config opening_interior;
    if(std::filesystem::exists("opening/interior.settings")){
        std::ifstream file("opening/interior.settings");std::string message;
        if(!file||!world_opening_interior::parse(file,opening_interior,message)){
            std::fprintf(stderr,"[OpeningInterior] Invalid settings: %s\n",message.c_str());return 1;
        }
        std::printf("[OpeningInterior] %zu authored interior probe domains; city fallback release %s\n",opening_interior.probes.size(),opening_interior.release_city_fallbacks?"enabled":"disabled");
    }
    uint64_t opening_house_shadow_frames=0;
    // Explicit exploration uses the same authored world without advancing its story.
    // Keep accidental unguarded demo launches rejected.
    const char *exploration_mode=std::getenv("VALWOOD_WORLD_EXPLORATION");
    const bool world_exploration=exploration_mode&&std::strcmp(exploration_mode,"1")==0;
    if(std::filesystem::is_regular_file("opening.config")&&!opening_enabled&&!launch->editor&&!world_exploration){std::fprintf(stderr,"[Opening] Use the Demo, World or Editor launcher for this project.\n");return 1;}
    active_opening_prologue=opening_enabled;
    struct OpeningClipScope {~OpeningClipScope(){active_opening_prologue=false;}} opening_clip_scope;
    const bool qa_town=presentation&&launch->frames&&!launch->benchmark&&std::getenv("VALWOOD_QA_TOWN");
    const bool qa_controller=presentation&&launch->frames&&!launch->benchmark&&std::getenv("VALWOOD_QA_CONTROLLER");
    const bool qa_grass=presentation&&launch->frames&&!launch->benchmark&&std::getenv("VALWOOD_QA_GRASS_INTERACTION");
    if(qa_grass){std::ifstream marker(".valwood-vegetation-qa");std::string token;std::getline(marker,token);if(token!="VALWOOD_SELDONNE_VEGETATION_QA_V1"){std::fprintf(stderr,"[Grass interaction QA] Private project marker missing\n");return 1;}}
    const bool qa_autowalk=std::getenv("VALWOOD_QA_AUTOWALK")!=nullptr;
    const bool qa_walking_controls=presentation&&launch->frames&&!launch->benchmark&&std::getenv("VALWOOD_QA_WALKING_CONTROLS");
    if(qa_walking_controls){std::ifstream marker(".valwood-walking-qa");std::string token;std::getline(marker,token);if(token!="VALWOOD_WALKING_QA_V1"){std::fprintf(stderr,"[Walking QA] Private project marker missing\n");return 1;}}
    const bool qa_minimap=presentation&&launch->frames&&!launch->benchmark&&std::getenv("VALWOOD_QA_MINIMAP");
    if(qa_minimap){std::ifstream marker(".valwood-minimap-qa");std::string token;std::getline(marker,token);if(token!="VALWOOD_MINIMAP_QA_V1"){std::fprintf(stderr,"[Minimap QA] Private project marker missing\n");return 1;}}
    world_minimap::Config minimap_config;world_minimap::Geometry minimap_geometry;MinimapStats minimap_stats;
    const bool minimap_present=std::filesystem::exists("minimap.config");bool minimap_valid=false;
    if(minimap_present){std::ifstream file("minimap.config");std::string error;minimap_valid=bool(file)&&world_minimap::parse(file,minimap_config,error);if(!minimap_valid)std::fprintf(stderr,"[Minimap] Disabled invalid config: %s\n",error.c_str());}
    unsigned minimap_revision=~0u;const void *minimap_ribbons=nullptr,*minimap_structures=nullptr;
    bool minimap_drawn=false;
    visual_surface_finish=std::filesystem::is_regular_file("visual_upgrade.json");visual_grass_interactor={};grass_interaction={};grass_player_movement_queries=grass_player_movement_query_failures=0;
    milestone_finish=std::filesystem::is_regular_file("milestone_standard.json");
    stronghold_finish=std::filesystem::is_regular_file("stronghold.json");
    source_showcase=std::filesystem::is_regular_file("publisher_showcase.json");
    city_showcase=std::filesystem::is_regular_file("city_showcase.json");
    const char *playtest_area=std::getenv("VALWOOD_PLAYTEST_SLICE");
    const bool playtest_enabled=playtest_area&&*playtest_area;
    const bool joust_enabled=presentation&&playtest_enabled&&std::getenv("VALWOOD_JOUST_ARENA")&&std::filesystem::is_regular_file("joust.enabled");
    const bool arena_enabled=!joust_enabled&&presentation&&playtest_enabled&&std::getenv("VALWOOD_COMBAT_ARENA")&&std::filesystem::is_regular_file("arena.config");
    const bool joust_qa=joust_enabled&&launch->frames&&!launch->benchmark&&std::getenv("VALWOOD_QA_JOUST");
    // Arena scenery is explicit project content; legacy test pads remain lean.
    const bool arena_scenery_enabled=(arena_enabled||joust_enabled)&&std::filesystem::is_regular_file("scenery/arena-woodland.enabled");
    const bool severing_qa=arena_enabled&&launch->frames&&!launch->benchmark&&std::getenv("VALWOOD_QA_SEVERING");
    const bool duel_qa=arena_enabled&&launch->frames&&!launch->benchmark&&std::getenv("VALWOOD_QA_DUEL");
    const bool arena_qa=arena_enabled&&launch->frames&&!launch->benchmark&&(std::getenv("VALWOOD_QA_ARENA")||severing_qa||duel_qa);
    if(severing_qa){std::ifstream marker(".valwood-severing-qa");std::string token;std::getline(marker,token);
        if(token!="VALWOOD_SEVERING_QA_V1"){std::fprintf(stderr,"[Severing QA] Private project marker missing\n");return 1;}}
    active_combat_arena=arena_enabled||joust_enabled;
    struct ArenaScope {~ArenaScope(){active_combat_arena=false;}} arena_scope;
    constexpr double arena_origin_x=-1800.,arena_origin_z=-10500.,arena_floor=64.;
    if(std::filesystem::exists("arena.config")&&!arena_enabled&&!joust_enabled){std::fprintf(stderr,"[Arena] Use the combat arena launcher.\n");return 1;}
    if(std::filesystem::is_regular_file("../local-origin.json")&&std::filesystem::is_regular_file("playtest.route")&&!playtest_enabled){
        std::fprintf(stderr,"[Playtest] This lightweight copy requires its guarded playtest launcher. Open the original artist workspace for authoring.\n");return 1;
    }
    const bool playtest_qa=playtest_enabled&&launch->frames&&launch->benchmark&&std::getenv("VALWOOD_QA_PLAYTEST");
    const bool playtest_input_qa=playtest_enabled&&launch->frames&&!launch->benchmark&&std::getenv("VALWOOD_QA_PLAYTEST_INPUT");
    const bool playtest_combat_qa=playtest_enabled&&launch->frames&&!launch->benchmark&&std::getenv("VALWOOD_QA_PLAYTEST_COMBAT");
    const bool qa_stick_combat=playtest_enabled&&launch->frames&&!launch->benchmark&&std::getenv("VALWOOD_QA_SWORD_STICK");
    const bool grandstone_obstacle_qa=playtest_enabled&&stronghold_finish&&launch->frames&&!launch->benchmark&&std::getenv("VALWOOD_QA_GRANDSTONE_OBSTACLES");
    world_playtest_slice::State playtest_state;
    std::unique_ptr<world_playtest_walk::Support> playtest_walk;
    struct PlaytestWalkScope {~PlaytestWalkScope(){active_playtest_walk=nullptr;active_structure_world=false;}} playtest_walk_scope;
    active_playtest_walk=nullptr;
    // A source catalog selects the authoritative scene-wide collision service.
    // Do not require or build obsolete fixed sectors before that service loads;
    // its later catalog/geometry validation fails clearly on malformed data.
    const bool source_collision_catalog=std::filesystem::exists("collision/structures/catalog");
    if(playtest_enabled){
        const bool matching=(!std::strcmp(playtest_area,"city")&&city_showcase)||
            (!std::strcmp(playtest_area,"jungle")&&std::filesystem::is_regular_file("jungle_expansion.json"))||
            (!std::strcmp(playtest_area,"grandstone")&&stronghold_finish&&source_showcase);
        world_playtest_slice::Path route;std::string error;
        if(!presentation||launch->editor||!matching||!world_playtest_slice::load("playtest.route",route,error)||!playtest_state.start(route,&error)){
            std::fprintf(stderr,"[Playtest] Matching project, play mode and valid route required: %s\n",error.c_str());return 1;
        }
        playtest_walk_reference=route.spawn.eye_y-1.75;
        if(stronghold_finish&&!source_collision_catalog){
            playtest_walk=std::make_unique<world_playtest_walk::Support>();
            if(!playtest_walk->load("playtest/approach.settings",error)){std::fprintf(stderr,"[Playtest] Collision rejected: %s\n",error.c_str());return 1;}
            float floor=0;
            if(!playtest_walk->sample(route.spawn.x,route.spawn.z,route.spawn.eye_y-1.75,floor)||std::abs(floor-(route.spawn.eye_y-1.75))>.15){std::fprintf(stderr,"[Playtest] Route spawn does not match its collision floor\n");return 1;}
            active_playtest_walk=playtest_walk.get();
        }
    }
    city_look={};active_city_physics=nullptr;active_city_sky=nullptr;
    std::unique_ptr<world_physics::World> city_physics;
    struct CityPhysicsScope {~CityPhysicsScope(){active_city_physics=nullptr;}} city_physics_scope;
    if(city_showcase){
        std::string error;
        if(!source_showcase||!world_city_scene::load("city.scene",city_look,error)){
            std::fprintf(stderr,"[City] Source look rejected: %s\n",error.c_str());return 1;
        }
        if(!source_collision_catalog&&std::filesystem::exists("city.physics")){
            world_physics::Document document;
            city_physics=std::make_unique<world_physics::World>();
            if(!world_physics::load("city.physics",document,error)||!city_physics->configure(document,error)){
                std::fprintf(stderr,"[City] Source collision rejected: %s\n",error.c_str());return 1;
            }
            active_city_physics=city_physics.get();
            std::printf("[City] Authored static collision: %zu bodies\n",document.bodies.size());
        }
    }
    // The desktop's X11 raw-input grab stalls5 seconds even while focused.
    // SDL's documented warp-relative implementation preserves relative deltas
    // and avoids that synchronous grab. Respect explicit user/environment hints
    // and restore the default when this viewer leaves, including early errors.
    struct RelativeMouseCompatibility {
        bool changed=false,warmed=false;
        RelativeMouseCompatibility(){
            const char *driver=SDL_GetCurrentVideoDriver();
            if(driver&&!std::strcmp(driver,"x11")&&!SDL_GetHint(SDL_HINT_MOUSE_RELATIVE_MODE_WARP)){
                changed=SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_MODE_WARP,"1")==SDL_TRUE;
                if(changed)std::printf("[World] Mouse capture: X11 warp-relative compatibility\n");
            }
        }
        void warm(SDL_Window *window){
            if(!changed||warmed||!(SDL_GetWindowFlags(window)&SDL_WINDOW_INPUT_FOCUS))return;
            const Uint64 t=SDL_GetPerformanceCounter();
            if(SDL_SetRelativeMouseMode(SDL_TRUE)==0){SDL_SetRelativeMouseMode(SDL_FALSE);warmed=true;}
            std::printf("[World] Input capture startup preparation %.3fms\n",1000.*double(SDL_GetPerformanceCounter()-t)/SDL_GetPerformanceFrequency());
        }
        ~RelativeMouseCompatibility(){if(changed){SDL_SetRelativeMouseMode(SDL_FALSE);SDL_ResetHint(SDL_HINT_MOUSE_RELATIVE_MODE_WARP);}}
    };
    Uint64 startup = SDL_GetPerformanceCounter();
    WorldGraphicsSettings graphics=world_graphics_settings_default();
    const char *graphics_settings_path=launch->frames?std::getenv("VALWOOD_QA_GRAPHICS_FILE"):nullptr;
    if(!graphics_settings_path||!*graphics_settings_path)graphics_settings_path="graphics.settings";
    char graphics_message[256]={};
    const auto graphics_loaded=world_graphics_settings_load(graphics_settings_path,&graphics,graphics_message,sizeof(graphics_message));
    if(graphics_loaded==WorldGraphicsSettingsLoadResult::Rejected||graphics_loaded==WorldGraphicsSettingsLoadResult::IOError)
        std::fprintf(stderr,"[Graphics] %s; using Balanced defaults\n",graphics_message);
    if(launch->graphics_preset[0]) {
        const auto preset=!std::strcmp(launch->graphics_preset,"performance")?WorldGraphicsPreset::Performance:
            !std::strcmp(launch->graphics_preset,"cinematic")?WorldGraphicsPreset::Cinematic:WorldGraphicsPreset::Balanced;
        world_graphics_settings_apply_preset(&graphics,preset);
    }
    if(launch->auto_quality>=0)graphics.auto_quality=launch->auto_quality!=0;
    if(launch->target_fps>0)graphics.target_fps=launch->target_fps;
    if(launch->render_scale>0){graphics.render_scale=launch->render_scale;graphics.min_render_scale=std::min(graphics.min_render_scale,graphics.render_scale);graphics.preset=WorldGraphicsPreset::Custom;}
    const auto resident_character_textures=graphics.character_textures;
    const auto resident_environment_textures=graphics.environment_textures;
    environment_texture_edge=world_environment_texture_edge(resident_environment_textures);
    std::printf("[Environment textures] Loaded %s, scenery limit %u (restart required)\n",world_environment_textures_name(resident_environment_textures),environment_texture_edge);
    world_character_quality::StoryAppearances resident_story_appearances;
    std::string character_quality_error;
    if(!world_character_quality::resolve_story(std::getenv("VALWOOD_CHARACTER_QUALITY_ROOT"),resident_character_textures,resident_story_appearances,character_quality_error)){
        std::fprintf(stderr,"[Characters/models] %s\n",character_quality_error.c_str());return 1;
    }
    if(resident_story_appearances.selected)
        std::printf("[Characters/models] Loaded %s geometry and texture profile; civilian=%s; armored=%s\n",world_character_textures_name(resident_character_textures),resident_story_appearances.civilian.c_str(),resident_story_appearances.armored.c_str());
    std::string character_texture_error;
    if(!world_character_textures::configure("character-textures",unsigned(resident_character_textures),character_texture_error)){
        std::fprintf(stderr,"[Character textures] %s\n",character_texture_error.c_str());return 1;
    }
    std::printf("[Character textures] Loaded %s, color limit %u, registered source images %zu\n",world_character_textures_name(resident_character_textures),world_character_texture_edge(resident_character_textures),world_character_textures::entries.size());
    foliage_detail_scale=world_foliage_distance_scale(graphics.foliage_distance);
    foliage_density_scale=world_foliage_density_scale(graphics.foliage_density);foliage_budget_tier=graphics.foliage_density;
    WorldGraphicsSettings graphics_draft=graphics,graphics_pending=graphics;
    std::printf("[Graphics] Foliage draw distance %s (%.2fx)\n",world_foliage_distance_name(graphics.foliage_distance),foliage_detail_scale);
    bool show_graphics=launch->graphics_menu,graphics_apply=false,display_resize=false;
    int display_w=launch->width,display_h=launch->height;
    uint64_t quality_epoch=1,quality_valid_after=120,quality_reallocations=0,quality_apply_failures=0;


    if (!world_atlas_init()) {
        std::fprintf(stderr, "[World] Atlas initialization failed\n");
        return 1;
    }
    char region_error[256]={};
    const int source_terrain=world_atlas_load_height_region("ground/showcase.height",region_error,sizeof region_error);
    if(source_terrain<0){std::fprintf(stderr,"[World] Source terrain rejected: %s\n",region_error);world_atlas_shutdown();return 1;}
    if(source_terrain>0){WorldAtlasHeightRegionInfo info={};world_atlas_height_region_info(&info);std::printf("[World] Authored source terrain %ux%u, %.1fx%.1fm\n",info.columns,info.rows,(info.columns-1)*info.stepx,(info.rows-1)*info.stepz);}
    if(city_showcase){
        const int surrounding=world_atlas_load_fallback_height_region("ground/countryside.height",region_error,sizeof region_error);
        if(surrounding<0){std::fprintf(stderr,"[City] Countryside terrain rejected: %s\n",region_error);world_atlas_shutdown();return 1;}
        if(surrounding>0){WorldAtlasHeightRegionInfo info={};world_atlas_fallback_height_region_info(&info);std::printf("[City] Surrounding terrain %ux%u, %.1fx%.1fm; original source height has priority\n",info.columns,info.rows,(info.columns-1)*info.stepx,(info.rows-1)*info.stepz);}
    }

    AssetBrowserQA browser_qa;
    EditorLongSessionQA long_session_qa;
    EditorStructureSplineQA structure_spline_qa;
    Studio studio(launch->frames?std::getenv("VALWOOD_QA_STUDIO_DOCUMENT"):nullptr);studio.open=launch->editor||(!studio.loaded&&access(studio.path.c_str(),F_OK)==0);studio.bind();studio_document=&studio.live;studio_shader_features=!studio.live.materials.empty()||!studio.live.lights.empty()||!studio.live.decals.empty();studio_environment_features=studio.live.environment.enabled;studio_water_features=!studio.live.waters.empty();studio_imported_features=WorldSurfaceGPU::features(studio.live.surfaces);studio_wind_features=!studio.live.surfaces.winds.empty();
    studio_graph_code={};if(!studio.live.graphs.empty()&&!world_graph::compile(studio.live.graphs,studio_graph_code,studio.message)){studio.live.graphs.clear();studio.dirty=studio.open=true;}

    if (launch->landmark[0]) {
        if (!std::strcmp(launch->landmark, "callicut"))
            std::snprintf(launch->landmark, sizeof(launch->landmark), "callieut");
        size_t n;
        const auto *places = world_atlas_landmarks(&n);
        bool found = false;
        for (size_t i = 0; i < n; i++)
            if (!std::strcmp(places[i].id, launch->landmark))
                found = true;
        if (!found) {
            std::fprintf(stderr, "[World] Unknown landmark: %s\n", launch->landmark);
            world_atlas_shutdown();
            return 2;
        }

    }

    SDL_setenv("EDITOR_UI_BACKEND", "imgui", 1);
    EditorUIState ui = {};

    editor_ui_init(&ui);
    AppBootstrapConfig cfg = {};
    // Preserve selected-project bus gains across the mixer initialization.
    audio_get_volumes(&cfg.master_vol,&cfg.music_vol,&cfg.sfx_vol);

    cfg.title = stronghold_finish?"Legends of Valwood | Grandstone Stronghold":presentation?"Legends of Valwood | A Place in the Watch":launch->editor?"Valwood Studio | Streamed World":project->name;
    cfg.win_w = launch->width;
    cfg.win_h = launch->height;
    cfg.editor_ui = &ui;
    AppBootstrapResult app = {};

    if (!app_bootstrap_run(&cfg, &app)) {
        world_atlas_shutdown();
        return 1;
    }

    SDL_Window *window = app.window;
    // Loading UI has no simulation clock and does not invent a progress percentage.
    // Keep it live during synchronous model imports on this same GL thread.
    const bool demo_presentation_enabled=opening_enabled&&!(std::getenv("VALWOOD_DEMO_PRESENTATION")&&!std::strcmp(std::getenv("VALWOOD_DEMO_PRESENTATION"),"0"));
    GLuint demo_logo=demo_presentation_enabled&&std::getenv("VALWOOD_STUDIO_LOGO")?texture(std::getenv("VALWOOD_STUDIO_LOGO"),false):0;
    struct StartupPlate {
        SDL_Window *window;EditorUIState *ui;GLuint logo;bool enabled;Uint64 last=0,begin=SDL_GetTicks64();const char *phase="Preparing the world";
        void draw(bool force=false){
            if(!enabled||(!force&&SDL_GetTicks64()-last<150))return;last=SDL_GetTicks64();SDL_PumpEvents();
            GLint draw=0,read=0,viewport[4];GLfloat clear[4];
            glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&draw);glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&read);glGetIntegerv(GL_VIEWPORT,viewport);glGetFloatv(GL_COLOR_CLEAR_VALUE,clear);
            int w=0,h=0;SDL_GL_GetDrawableSize(window,&w,&h);if(w<=0||h<=0)return;
            glBindFramebuffer(GL_FRAMEBUFFER,0);glViewport(0,0,w,h);glClearColor(.02f,.03f,.04f,1);glClear(GL_COLOR_BUFFER_BIT);
            editor_ui_begin_frame(ui);world_demo_presentation::loading((ImTextureID)(intptr_t)logo,"LEGENDS OF VALWOOD",phase,false,(last-begin)*.001);editor_ui_render(ui);SDL_GL_SwapWindow(window);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER,draw);glBindFramebuffer(GL_READ_FRAMEBUFFER,read);glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);glClearColor(clear[0],clear[1],clear[2],clear[3]);
        }
    } startup_plate{window,&ui,demo_logo,demo_presentation_enabled&&!launch->hidden};
    startup_plate.draw(true);
    struct LoadingObserverScope {~LoadingObserverScope(){world_model_set_load_observer(nullptr,nullptr);}} loading_observer_scope;
    world_model_set_load_observer([](void *p){static_cast<StartupPlate*>(p)->draw();},&startup_plate);

    if(!launch->frames&&!graphics_display(window,graphics)){
        std::snprintf(graphics_message,sizeof(graphics_message),"Saved display mode unavailable; using a window on this display.");
        SDL_SetWindowFullscreen(window,0);
    }
    int graphics_monitor_refresh=graphics_refresh_hz(window);
    // Foliage depth pre-pass (AAA candidate). Measured ~2.7 ms slower on the
    // forest camp (cost is shading, not overdraw), so opt-in: VALWOOD_FOLIAGE_PREPASS=1.
    const bool foliage_depth_prepass=std::getenv("VALWOOD_FOLIAGE_PREPASS")&&std::strcmp(std::getenv("VALWOOD_FOLIAGE_PREPASS"),"1")==0; // measured slower at the camp (52.2 vs 47.6 ms)
    unsigned long long foliage_prepass_frames=0;
    QualityController quality;quality.reset({graphics.auto_quality,graphics.render_scale,graphics.min_render_scale,float(world_graphics_effective_target_fps(graphics,graphics_refresh_hz(window)))},quality_epoch);
    float rendered_scale=graphics.auto_quality?quality.scale():graphics.render_scale;
    float minimum_rendered_scale=rendered_scale,maximum_rendered_scale=rendered_scale;
    // Automated captures must not resize or overwrite the user's Studio layout.
    if(launch->frames&&ImGui::GetCurrentContext())ImGui::GetIO().IniFilename=nullptr;
    if(studio_environment_features){std::string error;studio_environment=world_environment_create(studio.live.environment.path.c_str(),error);if(!studio_environment){studio.live.environment.enabled=false;studio_environment_features=false;studio.dirty=studio.open=true;studio.message="Saved environment could not load: "+error;}}

    RelativeMouseCompatibility relative_mouse_compatibility;
    SDL_GL_GetDrawableSize(window,&display_w,&display_h);
    int logical_w=display_w,logical_h=display_h;SDL_GetWindowSize(window,&logical_w,&logical_h);
    auto designer_view=studio.designer_viewport(float(logical_w),float(logical_h));
    auto designer_drawable=world_designer_viewport::drawable_rect(designer_view,float(logical_w),float(logical_h),display_w,display_h);
    int scene_display_w=std::max(1,designer_drawable.w),scene_display_h=std::max(1,designer_drawable.h);
    auto refresh_designer_view=[&](){
        const auto next=studio.designer_viewport(float(logical_w),float(logical_h));
        const auto drawable=world_designer_viewport::drawable_rect(next,float(logical_w),float(logical_h),display_w,display_h);
        if(designer_view.x!=next.x||designer_view.y!=next.y||designer_view.w!=next.w||designer_view.h!=next.h||designer_drawable.w!=drawable.w||designer_drawable.h!=drawable.h){display_resize=true;}
        designer_view=next;designer_drawable=drawable;scene_display_w=std::max(1,drawable.w);scene_display_h=std::max(1,drawable.h);
    };
    if (launch->hidden)
        SDL_HideWindow(window);
    std::printf("[World] Renderer %s\n", glGetString(GL_RENDERER));
    GLint fragment_texture_units=0;glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS,&fragment_texture_units);
    local_light_shadows_supported=fragment_texture_units>=17;
    local_light_shadow=LocalLightShadow{};
    Program land, water, post, foliage, foliageShadow;WorldLivePrograms live_programs;
    const char *qa_terrain_fragment=std::getenv("VALWOOD_QA_TERRAIN_FRAGMENT");
    const char *qa_foliage_fragment=std::getenv("VALWOOD_QA_FOLIAGE_FRAGMENT");
    bool shaders = land.load("shaders/world_terrain.vs.glsl",
        qa_terrain_fragment&&*qa_terrain_fragment?qa_terrain_fragment:"shaders/world_terrain.fs.glsl") &&
        water.load("shaders/world_water.vs.glsl", "shaders/world_water.fs.glsl") && post.load("shaders/world_post.vs.glsl",
        "shaders/world_post.fs.glsl") && foliage.load("shaders/world_foliage.vs.glsl",
        qa_foliage_fragment&&*qa_foliage_fragment?qa_foliage_fragment:"shaders/world_foliage.fs.glsl",false,false,
        qa_foliage_fragment&&*qa_foliage_fragment?Program::GrassRole::None:Program::GrassRole::Owner) && foliageShadow.load("shaders/world_foliage.vs.glsl",
        "shaders/world_foliage_shadow.fs.glsl") && live_programs.load();
    if (!shaders) {
        std::fprintf(stderr, "[World] Shader initialization failed\n");
        editor_ui_shutdown(&ui);
        audio_shutdown();
        SDL_GL_DeleteContext(app.gl_context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        world_atlas_shutdown();
        return 1;
    }
    static_assert(std::size(world_castle_creek_points)<=32,"Creek shader point capacity");
    const bool zero_coverage_cull_enabled=!(qa_foliage_fragment&&*qa_foliage_fragment)&&
        std::getenv("VALWOOD_QA_DISABLE_ZERO_COVERAGE_CULL")==nullptr;
    const bool shadow_gather_supported=SDL_GL_ExtensionSupported("GL_ARB_texture_gather")==SDL_TRUE;
    const bool shadow_gather_enabled=shadow_gather_supported&&std::getenv("VALWOOD_QA_ENABLE_SHADOW_GATHER")&&
        std::getenv("VALWOOD_QA_DISABLE_SHADOW_GATHER")==nullptr;
    land.use();land.i("uShadowGather",shadow_gather_enabled?1:0);
    foliage.use();foliage.i("uArchitectureWeathering",1);foliage.i("uColorCoverageCull",zero_coverage_cull_enabled?1:0);
    foliage.i("uShadowGather",shadow_gather_enabled?1:0);
    foliage.v2("uTreeNearFade",WORLD_TREE_NEAR_FADE_BEGIN,WORLD_TREE_NEAR_FADE_END);
    foliageShadow.use();foliageShadow.i("uColorCoverageCull",0);
    std::array<float,std::size(world_castle_creek_points)*3> creek_points={};
    for(size_t i=0;i<std::size(world_castle_creek_points);i++) {
        creek_points[i*3]=float(world_castle_creek_points[i].x);
        creek_points[i*3+1]=float(world_castle_creek_points[i].z);
        creek_points[i*3+2]=float(world_castle_creek_points[i].half_width);
    }
    double creek_origin_x,creek_origin_z;world_atlas_village_origin(&creek_origin_x,&creek_origin_z);
    water.use();water.v2("uCastleCreekOrigin",float(creek_origin_x),float(creek_origin_z));
    water.i("uCastleCreekPointCount",int(std::size(world_castle_creek_points)));
    glUniform3fv(water.at("uCastleCreekPoints[0]"),GLsizei(std::size(world_castle_creek_points)),creek_points.data());
    rainforest_materials(land);
    static_assert(WORLD_VILLAGE_FARM_COUNT<=4,"Cultivated terrain shader capacity");
    std::array<float,WORLD_VILLAGE_FARM_COUNT*4> farm_bounds={};
    std::array<float,WORLD_VILLAGE_FARM_COUNT*2> farm_rows={};
    for(size_t i=0;i<WORLD_VILLAGE_FARM_COUNT;i++) {
        const auto &f=world_village_farm_fields[i];
        farm_bounds[i*4]=float(f.xmin);farm_bounds[i*4+1]=float(f.zmin);
        farm_bounds[i*4+2]=float(f.xmax);farm_bounds[i*4+3]=float(f.zmax);
        farm_rows[i*2]=float(f.row_spacing);farm_rows[i*2+1]=float(f.row_phase);
    }
    land.use();land.i("uFarmCount",int(WORLD_VILLAGE_FARM_COUNT));
    land.v2("uFarmOriginXZ",float(creek_origin_x),float(creek_origin_z));
    glUniform4fv(land.at("uFarmBounds[0]"),GLsizei(WORLD_VILLAGE_FARM_COUNT),farm_bounds.data());
    glUniform2fv(land.at("uFarmRows[0]"),GLsizei(WORLD_VILLAGE_FARM_COUNT),farm_rows.data());


    WorldLivePrograms previous_live;
    Program previous_land,previous_water,previous_foliage,previous_shadow;bool programs_pending=false,previous_features=false,previous_environment_flag=false,previous_water_flag=false,previous_imported_flag=false,previous_wind_flag=false;world_graph::Code previous_graph_code;
    auto finish_programs=[&](bool commit){if(!programs_pending)return;if(!commit){live_programs.free();live_programs=std::move(previous_live);land.free();water.free();foliage.free();foliageShadow.free();land=std::move(previous_land);water=std::move(previous_water);foliage=std::move(previous_foliage);foliageShadow=std::move(previous_shadow);studio_shader_features=previous_features;studio_environment_features=previous_environment_flag;studio_water_features=previous_water_flag;studio_imported_features=previous_imported_flag;studio_wind_features=previous_wind_flag;studio_graph_code=std::move(previous_graph_code);}else{previous_live.free();previous_land.free();previous_water.free();previous_foliage.free();previous_shadow.free();}programs_pending=false;++world_program_epoch;world_environment_invalidate_programs(studio_environment);};
    auto reload_studio_shaders=[&](bool features,bool environment=false,const world_studio::Document *document=nullptr,bool defer=false)->bool{
        const bool previous=studio_shader_features,previous_environment=studio_environment_features,previous_water_feature=studio_water_features,previous_imported=studio_imported_features,previous_wind=studio_wind_features;auto old_code=studio_graph_code;world_graph::Code next_code;
        if(document&&!document->graphs.empty()&&!world_graph::compile(document->graphs,next_code,studio.message))return false;
        studio_shader_features=features;studio_environment_features=environment;studio_water_features=document&&!document->waters.empty();studio_imported_features=document&&WorldSurfaceGPU::features(document->surfaces);studio_wind_features=document&&!document->surfaces.winds.empty();studio_graph_code=std::move(next_code);
        WorldLivePrograms next_live;Program next_land,next_water,next_foliage,next_shadow;
        const bool ok=next_land.load("shaders/world_terrain.vs.glsl",qa_terrain_fragment&&*qa_terrain_fragment?qa_terrain_fragment:"shaders/world_terrain.fs.glsl",studio.fail_shader_test)&&
            next_water.load("shaders/world_water.vs.glsl","shaders/world_water.fs.glsl")&&
            next_foliage.load("shaders/world_foliage.vs.glsl",qa_foliage_fragment&&*qa_foliage_fragment?qa_foliage_fragment:"shaders/world_foliage.fs.glsl",false,false,
                qa_foliage_fragment&&*qa_foliage_fragment?Program::GrassRole::None:Program::GrassRole::Owner)&&
            next_shadow.load("shaders/world_foliage.vs.glsl","shaders/world_foliage_shadow.fs.glsl")&&next_live.load();
        if(!ok){next_live.free();next_land.free();next_water.free();next_foliage.free();next_shadow.free();studio_shader_features=previous;studio_environment_features=previous_environment;studio_water_features=previous_water_feature;studio_imported_features=previous_imported;studio_wind_features=previous_wind;studio_graph_code=std::move(old_code);return false;}
        ++world_program_epoch;world_environment_invalidate_programs(studio_environment);
        if(defer){previous_live=std::move(live_programs);previous_land=std::move(land);previous_water=std::move(water);previous_foliage=std::move(foliage);previous_shadow=std::move(foliageShadow);previous_features=previous;previous_environment_flag=previous_environment;previous_water_flag=previous_water_feature;previous_imported_flag=previous_imported;previous_wind_flag=previous_wind;previous_graph_code=std::move(old_code);programs_pending=true;}else{live_programs.free();land.free();water.free();foliage.free();foliageShadow.free();}
        live_programs=std::move(next_live);land=std::move(next_land);water=std::move(next_water);foliage=std::move(next_foliage);foliageShadow=std::move(next_shadow);
        land.use();land.i("uShadowGather",shadow_gather_enabled?1:0);rainforest_materials(land);
        land.i("uFarmCount",int(WORLD_VILLAGE_FARM_COUNT));land.v2("uFarmOriginXZ",float(creek_origin_x),float(creek_origin_z));
        glUniform4fv(land.at("uFarmBounds[0]"),GLsizei(WORLD_VILLAGE_FARM_COUNT),farm_bounds.data());glUniform2fv(land.at("uFarmRows[0]"),GLsizei(WORLD_VILLAGE_FARM_COUNT),farm_rows.data());
        foliage.use();foliage.i("uArchitectureWeathering",1);foliage.i("uColorCoverageCull",zero_coverage_cull_enabled?1:0);foliage.i("uShadowGather",shadow_gather_enabled?1:0);foliage.v2("uTreeNearFade",WORLD_TREE_NEAR_FADE_BEGIN,WORLD_TREE_NEAR_FADE_END);
        foliageShadow.use();foliageShadow.i("uColorCoverageCull",0);
        water.use();water.v2("uCastleCreekOrigin",float(creek_origin_x),float(creek_origin_z));water.i("uCastleCreekPointCount",int(std::size(world_castle_creek_points)));glUniform3fv(water.at("uCastleCreekPoints[0]"),GLsizei(std::size(world_castle_creek_points)),creek_points.data());
        return true;
    };
    // Frame-bounded QA only. Exercise the actual successful-link/failed-scene
    // rollback path, which pre-link failure tests never reach. Program moves
    // must transfer ownership rather than alias previous/next raw GL handles.
    if(launch->frames&&std::getenv("VALWOOD_QA_CHARACTER_PROGRAMS")){
        const auto ids=[&](){std::vector<GLuint> result{land.id,water.id,foliage.id,foliageShadow.id,live_programs.skeletal.id,live_programs.skeletal_shadow.id,live_programs.rigid.id,live_programs.rigid_shadow.id,live_programs.crowd.id,live_programs.crowd_shadow.id};if(foliage.generated_grass)result.push_back(foliage.generated_grass->id);if(foliage.vegetation)result.push_back(foliage.vegetation->id);if(foliage.depth_prepass)result.push_back(foliage.depth_prepass->id);return result;};
        const auto all_live=[](const auto &values){for(auto p:values)if(!p||!glIsProgram(p))return false;return true;};
        const auto none_live=[](const auto &values){for(auto p:values)if(p&&glIsProgram(p))return false;return true;};
        auto before=ids();const auto epoch=world_program_epoch;bool ok=all_live(before);
        if(ok)ok=reload_studio_shaders(studio_shader_features,studio_environment_features,&studio.live,true);
        const auto staged=ids();if(ok)ok=programs_pending&&all_live(staged)&&staged!=before;
        finish_programs(false);glUseProgram(0);ok=ok&&ids()==before&&all_live(before)&&none_live(staged)&&world_program_epoch==epoch+2;
        previous_live.free();previous_land.free();previous_water.free();previous_foliage.free();previous_shadow.free();glUseProgram(0);ok=ok&&all_live(before);
        studio.fail_shader_test=true;
        const bool rejected=reload_studio_shaders(studio_shader_features,studio_environment_features,&studio.live,true);studio.fail_shader_test=false;glUseProgram(0);
        ok=ok&&!rejected&&!programs_pending&&ids()==before&&all_live(before);
        if(ok)ok=reload_studio_shaders(studio_shader_features,studio_environment_features,&studio.live,true);
        const auto committed=ids();finish_programs(true);glUseProgram(0);ok=ok&&all_live(committed)&&none_live(before)&&!programs_pending;
        ok=ok&&glGetUniformLocation(foliage.id,"uCharacterRegion")==-1&&glGetUniformLocation(foliageShadow.id,"uCharacterEnabled")==-1&&glGetUniformLocation(live_programs.crowd.id,"uCharacterRegion")>=0&&glGetUniformLocation(live_programs.crowd_shadow.id,"uCharacterEnabled")>=0&&glGetError()==GL_NO_ERROR;
        std::printf("[Character program QA] %s:%zu unique owners, post-link rollback, moved-from cleanup, failed link, successful commit and isolated crowd variants\n",ok?"PASS":"FAIL",committed.size());
        if(!ok){live_programs.free();land.free();water.free();post.free();foliage.free();foliageShadow.free();editor_ui_shutdown(&ui);audio_shutdown();SDL_GL_DeleteContext(app.gl_context);SDL_DestroyWindow(window);SDL_Quit();world_atlas_shutdown();return 1;}
    }

    GLuint macro_height=0;
    GLuint materials[10] = {
        texture("ground/terrain_grass.png"), texture("ground/terrain_rock.png"), texture("ground/terrain_sand.png"), texture("ground/terrain_dirt.png"), texture("ground/terrain_grass_nor.png",
            false), texture("ground/terrain_rock_nor.png", false), climate_texture(macro_height), 0, texture("ground/terrain_grass_rgh.png", false), texture("ground/terrain_rock_rgh.png", false)
    };
    startup_plate.phase="Preparing sky, lighting and materials";startup_plate.draw(true);
    WorldCitySky city_visible_sky;
    struct CitySkyScope {~CitySkyScope(){active_city_sky=nullptr;}} city_sky_scope;
    bool city_visible_sky_ready=true;
    if(city_showcase){std::string error;const int loaded=city_visible_sky.load("city.sky",error);city_visible_sky_ready=loaded>=0;
        if(loaded<0)std::fprintf(stderr,"[City] Visible sky rejected: %s\n",error.c_str());
        if(loaded>0){active_city_sky=&city_visible_sky;std::printf("[City] Source visible sky: %upx, %.3fMiB; source brightness/contrast/rotation; material IBL unchanged\n",city_visible_sky.config.side,city_visible_sky.bytes/1048576.);}}
    WorldShowcaseTerrain showcase_terrain;
    const bool showcase_terrain_requested=std::filesystem::exists("ground/showcase_splat/terrain.txt");
    bool showcase_terrain_ready=true;
    if(showcase_terrain_requested){
        std::string error;showcase_terrain_ready=showcase_terrain.load("ground/showcase_splat/terrain.txt",error);
        if(!showcase_terrain_ready)std::fprintf(stderr,"[World] Source terrain materials rejected: %s\n",error.c_str());
        else std::printf("[World] Source terrain materials: 8 painted layers, %dpx PBR, %dpx weights, %.3f MiB in three arrays\n",
            showcase_terrain.side,showcase_terrain.alpha_side,showcase_terrain.bytes/1048576.);
    }

    // Slot7 remains owned/freed with materials; shader unit7 always uses the
    // real shadow map. The optional atlas UI fills this slot on first opening.
    bool atlas_map_attempted=false;
    WorldColorGrade village_grade;
    const bool village_grade_ready=village_grade.load("ground/village_grade.png");
    GLuint emptyVAO;
    glGenVertexArrays(1, &emptyVAO);
    Target target, reflection;target.separate_indirect=true;
    WorldSky *sky=world_sky_create();
    WorldOcclusion *occlusion=world_occlusion_create("shaders/world_post.vs.glsl","shaders/world_occlusion.fs.glsl",
        "shaders/world_occlusion_blur.fs.glsl","shaders/world_occlusion_compose.fs.glsl");
    // AAA merge: optional temporal upscaler (VALWOOD_TAAU=0 disables).
    WorldUpscale *upscaler=(std::getenv("VALWOOD_TAAU")&&std::strcmp(std::getenv("VALWOOD_TAAU"),"0")==0)?nullptr:
        world_upscale_create("shaders/world_post.vs.glsl","shaders/world_upscale.fs.glsl");
    // AAA merge: world ambience (fire, creek, wind, birds, footsteps). VALWOOD_AMBIENCE=0 disables.
    WorldAmbience *ambience=nullptr;double ambience_hearth[3]={};bool ambience_hearth_ready=false;
    if(!(std::getenv("VALWOOD_AMBIENCE")&&std::strcmp(std::getenv("VALWOOD_AMBIENCE"),"0")==0)&&std::getenv("VALWOOD_AMBIENCE_AUDIO")&&std::getenv("VALWOOD_SOUND_EFFECTS"))
        ambience=world_ambience_create(std::getenv("VALWOOD_AMBIENCE_AUDIO"),std::getenv("VALWOOD_SOUND_EFFECTS"));
    {
        std::ifstream camp_file("opening/arrival-camp.settings");world_opening_arrival_camp::Config camp_config;std::string camp_error;
        if(camp_file&&world_opening_arrival_camp::parse(camp_file,camp_config,camp_error)){
            ambience_hearth[0]=camp_config.center.x;ambience_hearth[1]=camp_config.center.y;ambience_hearth[2]=camp_config.center.z;ambience_hearth_ready=true;}
    }
    WorldUpscaleFrame upscale_frame;std::uint64_t upscale_frame_id=~0ull;int post_scene_w=0,post_scene_h=0;
    WorldBloom *bloom=world_bloom_create("shaders/world_post.vs.glsl","shaders/world_bloom_extract.fs.glsl",
        "shaders/world_bloom_blur.fs.glsl");
    const bool bloom_enabled=std::getenv("VALWOOD_QA_DISABLE_BLOOM")==nullptr;
    // The bloom mip chain returns the sum of six levels. Measured at the
    // campfire, .03 gives about twice the former glow at the core and a soft
    // halo out to ~250px; surfaces below the extract threshold gain nothing.
    // VALWOOD_BLOOM_STRENGTH overrides for look development.
    const float bloom_strength=bloom_enabled?(std::getenv("VALWOOD_BLOOM_STRENGTH")?std::clamp(float(std::atof(std::getenv("VALWOOD_BLOOM_STRENGTH"))),0.f,1.f):.03f):0.f;
    // Eye adaptation multiplies the authored exposure (which remains the
    // studio control); a failed setup simply leaves the multiplier at 1.
    WorldAutoExposure *auto_exposure=world_auto_exposure_create("shaders/world_post.vs.glsl","shaders/world_exposure_meter.fs.glsl");
    float auto_exposure_factor=1.f;
    // Per-object motion for temporal AA (player hero). Optional: on failure
    // the resolve keeps its camera-only reprojection.
    WorldObjectMotion *object_motion=std::getenv("VALWOOD_OBJECT_MOTION")&&!std::strcmp(std::getenv("VALWOOD_OBJECT_MOTION"),"0")?nullptr:
        world_object_motion_create("shaders/world_object_motion.vs.glsl","shaders/world_object_motion.fs.glsl");
    float motion_previous_vp[16]={};double motion_previous_camera[3]={},motion_previous_root[3]={};
    float motion_previous_yaw=0;bool motion_previous_valid=false;
    const void *motion_previous_identity=nullptr;std::uint64_t motion_previous_uploads=0,object_motion_skinned_frames=0;
    std::uint64_t object_motion_frames=0,object_motion_failures=0,npc_object_motion_actors=0;
    size_t bloom_failures=0;
    WorldWeather *weather=world_weather_create();
    const bool qa_bandit_lights_off=std::getenv("VALWOOD_QA_BANDIT_LIGHTS_OFF")!=nullptr;
    const VillageTorchEmitter polish_tavern_torches[]={
        {-150.,26.5,-89.62,.24f,.12f,18.f,91875,0,0,0,0,-154.7f,-89.7f,-135.3f,-74.3f,24.1f,28.3f},
        {-140.,26.5,-74.38,.24f,.12f,18.f,91876,0,0,0,0,-154.7f,-89.7f,-135.3f,-74.3f,24.1f,28.3f}
    };
    std::array<VillageTorchEmitter,std::size(village_torch_emitters)+std::size(castle_street_torch_emitters)+2> town_torch_emitters={};
    std::copy(std::begin(village_torch_emitters),std::end(village_torch_emitters),town_torch_emitters.begin());
    std::copy(std::begin(castle_street_torch_emitters),std::end(castle_street_torch_emitters),town_torch_emitters.begin()+std::size(village_torch_emitters));
    std::copy(std::begin(polish_tavern_torches),std::end(polish_tavern_torches),town_torch_emitters.end()-2);
    static_assert(std::size(village_torch_emitters)+std::size(castle_street_torch_emitters)+std::size(bandit_torch_emitters)+3<=WORLD_TORCH_FLAME_CAPACITY,"Torch emitter capacity");
    const VillageTorchEmitter trail_camp_torches[]={{0,WORLD_TRAIL_CAMP_Y+.24,0,.95f,.58f,108.f,91873,0,0,0,0,-10,-10,10,10,float(WORLD_TRAIL_CAMP_Y-.1),float(WORLD_TRAIL_CAMP_Y+9)}};
    WorldTorchFlames *torch_flames=world_torch_flames_create();
    const bool purchased_fire_ready=world_torch_flames_load_sprites(torch_flames,"particles/purchased_fire.vfx");
    std::vector<WorldTorchFlameEmitter> flame_emitters(std::size(village_torch_emitters)+std::size(castle_street_torch_emitters)+std::size(bandit_torch_emitters)+3);
    for(size_t i=0;i<std::size(village_torch_emitters);i++) {
        const auto &e=village_torch_emitters[i];
        flame_emitters[i]={creek_origin_x+e.x,e.y,creek_origin_z+e.z,e.height,e.width,e.intensity/45.f,e.seed};
    }
    for(size_t i=0;i<std::size(castle_street_torch_emitters);i++) {
        const auto &e=castle_street_torch_emitters[i];
        flame_emitters[std::size(village_torch_emitters)+i]={creek_origin_x+e.x,e.y,creek_origin_z+e.z,
            e.height,e.width,e.intensity/75.f,e.seed};
    }
    for(size_t i=0;i<2;i++){const auto &e=polish_tavern_torches[i];flame_emitters[town_torch_emitters.size()-2+i]={creek_origin_x+e.x,e.y,creek_origin_z+e.z,e.height,e.width,.40f,e.seed};}
    for(size_t i=0;i<std::size(bandit_torch_emitters);i++) {
        const auto &e=bandit_torch_emitters[i];
        flame_emitters[town_torch_emitters.size()+i]={bandit_camp_center_x+e.x,e.y,bandit_camp_center_z+e.z,
            e.height,e.width,qa_bandit_lights_off?0.f:e.intensity/75.f,e.seed};
    }
    flame_emitters.back()={WORLD_TRAIL_CAMP_X,WORLD_TRAIL_CAMP_Y+.24,WORLD_TRAIL_CAMP_Z,.95f,.58f,1.08f,91873};
    bool source_flames_enabled=false,source_flames_valid=true;
    if(source_showcase){
        std::string error;const auto status=world_torch_flames_read_file("showcase.flames",flame_emitters,error);
        source_flames_enabled=status==WorldTorchFlameFileStatus::Loaded;source_flames_valid=status!=WorldTorchFlameFileStatus::Invalid;
        if(!source_flames_valid)std::fprintf(stderr,"[World] Source flames: %s\n",error.c_str());
        if(source_flames_enabled)std::printf("[World] Authored source flames: %zu; actor-only practical lights, static baked lighting unchanged\n",flame_emitters.size());
    }
    if(battlefield_enabled){world_battlefield_scene::Director field;field.prepare();
        for(const auto &e:field.flames())flame_emitters.push_back(e);
    }
    // Freeze fixture sockets once; nearest-eight selection reuses fixed stack
    // storage each frame. A modest4-unit power is below the legacy20-unit torch.
    std::vector<VillageTorchEmitter> source_practicals;
    const double source_practical_x=source_flames_enabled&&!flame_emitters.empty()?flame_emitters.front().x:0.;
    const double source_practical_z=source_flames_enabled&&!flame_emitters.empty()?flame_emitters.front().z:0.;
    if(source_flames_enabled){
        source_practicals.reserve(flame_emitters.size());
        for(const auto &e:flame_emitters){
            const double x=e.x-source_practical_x,z=e.z-source_practical_z;
            source_practicals.push_back({x,e.y,z,e.height,e.width,4.f*e.intensity,e.seed,0,0,0,1,
                float(x-12),float(z-12),float(x+12),float(z+12),float(e.y-12),float(e.y+12)});
        }
    }
    auto source_actor_practicals=[&](const Camera &eye,float time,WorldTorchLights &lights){
        if(!source_flames_enabled)return true;
        if(!world_torch_lights_build(source_practicals.data(),source_practicals.size(),source_practical_x,source_practical_z,
            eye.x,eye.y,eye.z,time,lights,12.f,true))return false;
        studio_lights(lights,eye,time,studio.vfx_previewing?&studio.draft:nullptr,
            studio.vfx_previewing?studio.vfx_clock.time:-1,studio.vfx_previewing&&studio.vfx_solo?studio.particle_selection:-1);
        lights.actor_only=1;return true;
    };
    const bool torch_ready=source_flames_valid&&torch_flames&&world_torch_flames_set_emitters(torch_flames,flame_emitters.data(),flame_emitters.size());
    size_t torch_peak_lights=0,torch_peak_visible=0,torch_failures=0,torch_main_frames=0,torch_reflection_frames=0;
    size_t bandit_light_count=0,bandit_peak_lights=0,bandit_peak_flames=0;
    uint64_t bandit_flame_draws=0,last_torch_draws=0;

    WorldSceneParticles *scene_particles=world_scene_particles_create();
    std::array<WorldSceneParticleEmitter,3> camp_life={};
    const auto camp_life_count=world_scene_particles_camp_life(-11988.,11.8000001907,5172.,camp_life.data(),camp_life.size());
    world_scene_particles_set_environment_emitters(scene_particles,camp_life.data(),camp_life_count);
    const bool purchased_smoke_ready=world_scene_particles_load_smoke(scene_particles,"particles/purchased_smoke.vfx");
    // Authored sources, never a camera-following cloud. Chimney birth is above
    // the actual9.7m chimney outlet; no smoke is emitted by indoor lanterns.
    std::array<WorldSceneParticleEmitter,14> scene_emitters={};
    auto particle_source=[&](size_t i,double x,double y,double z,WorldSceneParticleKind kind,
                            float radius,float height,float strength,float warmth,uint32_t seed,double ceiling) {
        scene_emitters[i]={{x,y,z},kind,radius,height,strength,warmth,seed,ceiling};
    };
    particle_source(0,creek_origin_x-138.5,33.76,creek_origin_z-89.15,WorldSceneParticleKind::Smoke,2.3f,8.f,.95f,.10f,97101,43.);
    particle_source(1,creek_origin_x-58.00335,25.9,creek_origin_z-91.,WorldSceneParticleKind::Smoke,1.5f,5.5f,.48f,.7f,97102,32.);
    particle_source(2,creek_origin_x+52.29665,25.9,creek_origin_z-88.,WorldSceneParticleKind::Smoke,1.5f,5.5f,.48f,.7f,97103,32.);
    particle_source(3,creek_origin_x-176.,27.7,creek_origin_z+22.,WorldSceneParticleKind::Smoke,1.0f,3.5f,.35f,.5f,97104,32.);
    particle_source(4,bandit_camp_center_x,189.55,bandit_camp_center_z,WorldSceneParticleKind::Smoke,2.0f,7.f,.85f,.85f,97105,198.);
    particle_source(5,bandit_camp_center_x-20.,189.1,bandit_camp_center_z+5.,WorldSceneParticleKind::Smoke,1.6f,5.5f,.65f,.65f,97106,196.);
    particle_source(6,bandit_camp_center_x+5.,189.7,bandit_camp_center_z-31.,WorldSceneParticleKind::Smoke,1.4f,5.f,.5f,.65f,97107,196.);
    // Sparse mote volumes stay below the tavern ceiling and inside the street
    // corridors. Their real3–9mm size is energy filtered rather than enlarged.
    particle_source(7,creek_origin_x-145.,24.4,creek_origin_z-82.,WorldSceneParticleKind::Dust,5.8f,3.2f,.55f,0,97201,27.8);
    particle_source(8,creek_origin_x-72.,24.3,creek_origin_z+55.,WorldSceneParticleKind::Dust,3.0f,3.5f,.75f,0,97202,28.);
    particle_source(9,creek_origin_x-138.,24.3,creek_origin_z+16.,WorldSceneParticleKind::Dust,2.6f,3.2f,.65f,0,97203,27.8);
    particle_source(10,creek_origin_x-176.,24.3,creek_origin_z+22.,WorldSceneParticleKind::Dust,4.0f,3.2f,.65f,0,97204,27.8);
    particle_source(11,creek_origin_x-10.,24.3,creek_origin_z+35.,WorldSceneParticleKind::Dust,6.0f,4.f,.55f,0,97205,29.);
    particle_source(12,bandit_camp_center_x,188.3,bandit_camp_center_z+10.,WorldSceneParticleKind::Dust,5.f,3.8f,.55f,0,97206,193.);
    particle_source(13,WORLD_TRAIL_CAMP_X,WORLD_TRAIL_CAMP_Y+.8,WORLD_TRAIL_CAMP_Z,WorldSceneParticleKind::Smoke,1.5f,5.f,.50f,.65f,91874,WORLD_TRAIL_CAMP_Y+6.);
    const size_t scene_emitter_count=city_showcase?0:scene_emitters.size();
    const bool particles_ready=scene_particles&&world_scene_particles_set_emitters(scene_particles,scene_emitters.data(),scene_emitter_count);
    size_t particle_peak_smoke=0,particle_peak_dust=0;
    studio.base_particles.assign(scene_emitters.begin(),scene_emitters.begin()+scene_emitter_count);studio.base_flames.assign(flame_emitters.begin(),flame_emitters.end());

    size_t weather_failures=0,pool_frames=0,pool_reflection_frames=0,creek_frames=0,creek_reflection_frames=0;
    const bool weather_disabled=std::getenv("VALWOOD_DISABLE_WEATHER")!=nullptr;
    const bool contact_enabled=std::getenv("VALWOOD_DISABLE_OCCLUSION")==nullptr;
    GLuint latest_color=0;
    size_t occlusion_failures=0;
    WorldWaterScene *water_scene=world_water_scene_create();
    size_t water_scene_frames=0;
    uint64_t water_scene_peak_bytes=0;
    // Explicit visual-QA override compares the exact same scene and executable
    // against a preserved resolve shader. Ordinary play always uses the default.
    const char *qa_temporal_fragment=std::getenv("VALWOOD_QA_TEMPORAL_FRAGMENT");
    WorldTemporal *temporal=world_temporal_create("shaders/world_temporal.vs.glsl",
        qa_temporal_fragment&&*qa_temporal_fragment?qa_temporal_fragment:"shaders/world_temporal.fs.glsl");
    uint64_t temporal_sample=0;
    bool temporal_cut=true;
    float history_hour=17.05f,history_cloud=.28f,history_rain=.48f,history_wind=.85f,history_mist=.62f;
    float history_bearing=0,history_sun_strength=1.12f,history_sky_fill=1.f;
    Shadow shadow;
    FarShadow far_shadow;far_shadow.init();
    bool valid = shadow.init(graphics.shadow_side) && target.resize(std::max(1,int(std::lround(scene_display_w*rendered_scale))),std::max(1,int(std::lround(scene_display_h*rendered_scale)))) && temporal && water_scene && sky && occlusion && bloom && weather && village_grade_ready && torch_ready && particles_ready && purchased_fire_ready && purchased_smoke_ready && showcase_terrain_ready && city_visible_sky_ready;
    auto startup_status=[&](const char *stage){if(!valid){std::fprintf(stderr,"[World] Startup resource validation failed after %s\n",stage);launch->exit_code=1;}};
    startup_status("render targets and effects");
    latest_color=target.color;
    if(shadow.side!=graphics.shadow_side){graphics.shadow_side=shadow.side;graphics.preset=WorldGraphicsPreset::Custom;graphics_draft=graphics_pending=graphics;}
    TerrainGPU terrain;
    valid = terrain.init() && valid;
    startup_status("terrain stream");
    Dressing dressing;
    valid = dressing.init() && valid;
    startup_status("ground cover");
    world_cover_set_filter(dressing.cover,studio.live.stamps.empty()&&!studio.live.surface_field?nullptr:Studio::cover,&studio.live);
    auto landscape_publish_cover=[&](){studio.landscape_refresh_protection();return !dressing.cover?studio.live.ecology.empty():world_cover_set_regions(dressing.cover,studio.live.ecology.data(),studio.live.ecology.size(),source_showcase,Studio::landscape_live_protected,&studio);};
    valid=landscape_publish_cover()&&valid;
    CoastalOutcrops outcrops;
    valid = outcrops.init() && valid;
    startup_status("coastal outcrops");
    HorizonForest horizon;
    valid = horizon.init() && valid;
    LandscapeMaterials landscape;
    size_t landmark_count;
    const auto *landmarks = world_atlas_landmarks(&landmark_count);
    int current = 0;
    for (size_t i = 0; i < landmark_count; i++)
        if (!std::strcmp(landmarks[i].id, launch->landmark[0] ? launch->landmark : "seldonne"))
            current = (int) i;
    Camera cam;
    // AAA merge: the lean vegetation variant omits interior/home, castle,
    // tavern, storm-flash and armor paths. Use it only where none can apply.
    auto lean_vegetation_ok=[&]()->bool{
        if(stronghold_finish)return false;   // (Seldonne itself runs as a 'city showcase')
        auto near_box=[&](const std::array<float,4> &b,double margin){
            return cam.x>b[0]-margin&&cam.x<b[2]+margin&&cam.z>b[1]-margin&&cam.z<b[3]+margin;};
        if(landscape.castle_lights_ready&&near_box(landscape.castle_lights.bounds,160.))return false;
        if(landscape.tavern_lighting_ready&&near_box(landscape.tavern_lighting.bounds,160.))return false;
        return std::hypot(cam.x+3499.4,cam.z+11001.6)>400.;   // opening home interior
    };
    world_milestone::Review milestone_review;Camera milestone_return_camera;
    int milestone_return_screen=0;bool milestone_return_third=true,milestone_return_save=false;
    const char *milestone_return_name=nullptr;float portrait_focus_distance=0;
    // A capture-only override leaves ordinary play's default and preferences unchanged.
    const char *capture_view=std::getenv("VALWOOD_CAPTURE_VIEW");
    const bool force_capture_first=launch->frames&&capture_view&&!std::strcmp(capture_view,"first");
    const bool force_capture_arena=launch->frames&&std::getenv("VALWOOD_CAPTURE_ARENA_START");
    bool third_person=!force_capture_first;
    unsigned camera_toggles=0;float boom_distance=world_follow_camera_max_distance;
    WorldFollowCameraSafety follow_camera_safety;
    std::uint64_t camera_final_line_blocks=0,camera_eye_motion_blocks=0,camera_final_occluded_frames=0;
    world_character_presentation::StepHeight character_step_height;
    world_player_movement::State player_move;
    std::uint64_t arena_player_sweep_contacts=0,arena_player_sweep_failures=0;
    double player_move_last_x=cam.x,player_move_last_z=cam.z;
    bool player_move_initialized=false;
    float combat_shoulder_offset=0,combat_shoulder_peak=0;unsigned long long combat_shoulder_frames=0,combat_shoulder_blocks=0;
    bool combat_shoulder_active=false,combat_shoulder_safe=true;double combat_shoulder_target_x=0,combat_shoulder_target_y=0,combat_shoulder_target_z=0;
    WorldCombatFocusCamera combat_focus_camera;WorldCombatFocusFrame combat_focus_frame;
    WorldCombatCloseupCamera combat_closeup_camera;
    WorldExplorationCamera exploration_camera;
    bool combat_focus_applied=false;double combat_focus_goal_yaw=0,combat_focus_goal_pitch=0;
    double avatar_last_x=0,avatar_last_z=0;bool avatar_initialized=false;float avatar_yaw=0;
    cam.aspect=world_designer_viewport::aspect(designer_view);
    const bool pocket_benchmark=launch->benchmark&&starting_scenic(launch->scenic);
    // A named walking scene should open at its authored eye height in Studio
    // too. F still enables flight; unnamed atlas inspection starts overhead.
    cam.fly = launch->editor&&!starting_scenic(launch->scenic);
    teleport(cam, landmarks[current]);
    const char *current_name=landmarks[current].name;
    const char *demo_tour[]={"castle-finished-arcade","castle-finished-west","castle-market","castle-tavern-interior",
        "castle-creek-garden","castle-purchased-gardens","rainforest-purchased-understory","rainforest-camp","rainforest-pool","castle-frost-dragon","castle-dragons","castle-dragon-overlook","castle-dragon-flight"};
    unsigned demo_showcase_index=0;
    for(unsigned i=0;i<std::size(demo_tour);i++)if(!std::strcmp(launch->scenic,demo_tour[i]))demo_showcase_index=i;
    bool builtin_viewpoint=false;
    for (size_t i=0; i<sizeof(scenic_views)/sizeof(scenic_views[0]); i++)
        if (!std::strcmp(launch->scenic,scenic_views[i].id)) {
            scenic_travel(cam,unsigned(i)); current_name=scenic_views[i].name;builtin_viewpoint=true;
        }
    // Source-scene viewpoints retain the publisher's height and framing.
    // This optional project data never moves an ordinary project's camera.
    struct SourceView {std::string name;double x,y,z;float yaw,pitch,fov=1.0471975512f;};
    std::vector<SourceView> source_views;unsigned source_view_index=0;
    auto apply_source_view=[&](const SourceView &v){
        const bool walking=playtest_enabled||(city_showcase&&(v.name=="city-walk"||v.name=="city-approach"||v.name=="city-meadow"||v.name=="city-woodland"));
        cam.x=v.x;cam.z=v.z;cam.y=walking?walking_height(cam.x,cam.z,nullptr)+1.75:v.y;
        cam.yaw=v.yaw;cam.pitch=std::clamp(v.pitch,-1.5f,1.5f);cam.fov=v.fov;cam.fly=!walking;cam.grounded=walking;cam.vy=0;
    };
    bool source_viewpoint=false;
    if(std::ifstream views{"showcase.views"};views){
        std::string line;
        while(std::getline(views,line)){
            if(line.size()>512)continue;
            SourceView v;std::istringstream row(line);std::string extra;
            if(!(row>>v.name>>v.x>>v.y>>v.z>>v.yaw>>v.pitch)||v.name.size()>95)continue;
            row>>std::ws;if(!row.eof()&&!(row>>v.fov))continue;
            if(row>>extra)continue;
            if(source_views.size()>=64||!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.z)||!std::isfinite(v.yaw)||!std::isfinite(v.pitch)||!std::isfinite(v.fov)||v.fov<.15f||v.fov>2.8f||std::abs(v.x)>50400||std::abs(v.z)>28400||v.y< -2000||v.y>12000)continue;
            source_views.push_back(v);
            if(v.name!=launch->scenic)continue;
            source_view_index=unsigned(source_views.size()-1);apply_source_view(source_views.back());source_viewpoint=true;
            std::printf("[World] Publisher viewpoint %s %.3f %.3f %.3f FOV %.2f\n",v.name.c_str(),v.x,v.y,v.z,v.fov*57.2957795f);
        }
    }
    if(city_showcase&&!source_viewpoint&&!builtin_viewpoint&&(!launch->editor||!launch->scenic[0])){
        if(!source_views.empty())source_view_index=unsigned(source_views.size()-1);
        cam.x=project->spawn_x;cam.z=project->spawn_z;cam.y=walking_height(cam.x,cam.z,nullptr)+1.75;
        cam.yaw=project->yaw;cam.pitch=-.04f;cam.fly=false;cam.grounded=true;cam.vy=0;
        current_name="Publisher medieval city";
    }
    if(playtest_enabled){
        const auto &spawn=playtest_state.spawn();
        cam.x=spawn.x;cam.z=spawn.z;playtest_walk_reference=spawn.eye_y-1.75;
        cam.y=walking_height(cam.x,cam.z,nullptr)+1.75;cam.yaw=float(spawn.yaw);cam.pitch=float(spawn.pitch);
        cam.fly=false;cam.grounded=true;cam.vy=0;third_person=true;current_name=playtest_state.path().name.c_str();
    }
    world_source_review::Route source_review;
    if(const char *route=std::getenv("VALWOOD_QA_SOURCE_ROUTE")) {
        std::string error;
        if(!source_showcase||!launch->frames||!launch->benchmark||
            !source_review.load(route,std::getenv("VALWOOD_QA_SOURCE_ROUTE_REPORT"),std::getenv("VALWOOD_QA_SOURCE_ROUTE_CAPTURE"),error)) {
            std::fprintf(stderr,"[Source review] Explicit source project, bounded --frames, --benchmark and valid report path required: %s\n",error.c_str());valid=false;
        }else{
            source_review.begin(cam);source_viewpoint=true;third_person=false;current_name=source_review.label();
            std::printf("[Source review] %zu points, %u deterministic60Hz ticks; visual flight only\n",source_review.point_count(),source_review.requested_frames());
        }
    }
    // Capture-only material inspection uses the actual resident poses and
    // production lighting, close enough to inspect faces and clothing.
    if(stronghold_finish&&launch->frames)if(const char *portrait=std::getenv("VALWOOD_QA_DWARF_PORTRAIT")) {
        const unsigned stations[]={0,3,5,7,1};char *end=nullptr;const long index=std::strtol(portrait,&end,10);
        if(end&&!*end&&index>=0&&index<long(std::size(stations))){
            const auto &s=stronghold_stations[stations[index]];
            const double distance=2.35;
            cam.x=s.x+std::sin(s.yaw)*distance;cam.z=s.z+std::cos(s.yaw)*distance;
            cam.y=world_atlas_height(cam.x,cam.z)+1.15;
            cam.yaw=float(std::atan2(s.x-cam.x,cam.z-s.z));cam.pitch=-.06f;
            cam.fly=false;cam.fov=.70f;third_person=false;
        }
    }
    // Preparing an explicitly isolated opening starts at its existing authored
    // capsule, rather than at terrain height before the opening host is ready.
    if(opening_enabled&&opening_interior.enabled){
        std::ifstream file("opening.config");file.imbue(std::locale::classic());
        std::string tag,record;unsigned version=0;double x,y,z,yaw,pitch;
        if(!(file>>tag>>version>>record>>x>>y>>z>>yaw>>pitch)||tag!="VALWOOD_OPENING_SCENE"||version!=1||record!="house_spawn"||
           !world_opening_interior::contains(opening_interior,{x,y+1.75,z})||!std::isfinite(yaw)||!std::isfinite(pitch)){
            std::fprintf(stderr,"[OpeningInterior] Authored opening spawn must be inside the opted-in volume\n");return 1;
        }
        cam.x=x;cam.y=y+1.75;cam.z=z;cam.yaw=float(yaw);cam.pitch=float(pitch);
    }
    char scenery_error[256]={};
    std::vector<const char *> scenery_files={"scenery/authored_instances.txt","scenery/starting_area_instances.txt","scenery/rainforest_instances.txt","scenery/castle_village_instances.txt","scenery/tanthoon_bandit_camp_instances.txt","scenery/castle_streets_instances.txt","scenery/bep_environment_instances.txt","scenery/demo_finish_instances.txt","scenery/demo_polish_instances.txt"};
    if(access("scenery/battlefield_instances.txt",F_OK)==0)scenery_files.push_back("scenery/battlefield_instances.txt");
    if(access("scenery/demo_completion_instances.txt",F_OK)==0)scenery_files.push_back("scenery/demo_completion_instances.txt");
    const char *vegetation_overlay=std::getenv("VALWOOD_VEGETATION_DRESSING");
    if(vegetation_overlay&&*vegetation_overlay)scenery_files.push_back(vegetation_overlay);
    WorldSceneryStreamingConfig scenery_config;
    scenery_config.max_texture_edge=environment_texture_edge;
    scenery_config.texture_bytes=world_environment_texture_budget_bytes(resident_environment_textures);
    if(source_showcase){scenery_config.geometry_bytes=1280ull*1024*1024;scenery_config.cpu_bytes=768ull*1024*1024;scenery_config.preload_meters=64;scenery_config.unload_meters=96;}
    // The imported city houses prepare their full model and shadow proxy
    // together. Allow that transient CPU pair without enlarging GPU budgets.
    if(city_showcase)scenery_config.cpu_bytes=1280ull*1024*1024;
    // Loading boundary only, after initial targets/effects have allocated. The
    // same-context memory hints only enlarge the registered-image admission
    // pool on ample hardware; unavailable/3GB paths retain the legacy cap.
    const auto texture_pool_hints=world_texture_pool_policy::query(
        SDL_GL_ExtensionSupported("GL_NVX_gpu_memory_info")==SDL_TRUE,
        SDL_GL_ExtensionSupported("GL_ATI_meminfo")==SDL_TRUE,
        [](unsigned token,int *value){glGetIntegerv(GLenum(token),value);});
    const auto resident_texture_pool=world_texture_pool_policy::resolve(scenery_config.texture_bytes,
        texture_pool_hints,texture_cache_stats().gpu_bytes,scenery_config.geometry_bytes);
    const std::uint64_t resident_texture_pool_bytes=resident_texture_pool.bytes;
    scenery_config.texture_bytes=resident_texture_pool_bytes;
    std::printf("[Texture pool] Auto %llu MiB, legacy %llu MiB, hint=%s, initial-free %llu MiB, registered %llu MiB, driver-reserve %llu MiB, future-geometry-reserve %llu MiB; admission hint, not allocation guarantee (restart required)\n",
        static_cast<unsigned long long>(resident_texture_pool.bytes/(1024*1024)),
        static_cast<unsigned long long>(resident_texture_pool.legacy_bytes/(1024*1024)),
        world_texture_pool_policy::name(resident_texture_pool.source),
        static_cast<unsigned long long>(resident_texture_pool.initial_free_bytes/(1024*1024)),
        static_cast<unsigned long long>(resident_texture_pool.registered_start_bytes/(1024*1024)),
        static_cast<unsigned long long>(resident_texture_pool.driver_reserve_bytes/(1024*1024)),
        static_cast<unsigned long long>(resident_texture_pool.geometry_reserve_bytes/(1024*1024)));
    if(opening_interior.enabled&&opening_interior.release_city_fallbacks){
        scenery_config.interior_release_city_fallbacks=true;
        scenery_config.interior_start_active=world_opening_interior::contains(opening_interior,{cam.x,cam.y,cam.z});
        for(unsigned k=0;k<3;k++){scenery_config.interior_minimum[k]=opening_interior.bounds.minimum[k];scenery_config.interior_maximum[k]=opening_interior.bounds.maximum[k];}
    }
    auto load_initial_scenery=[&](){return std::getenv("VALWOOD_QA_DISABLE_ASSET_STREAMING")?world_scenery_load_files_budgeted(scenery_files.data(),std::size(scenery_files),"models",environment_texture_edge,scenery_error,sizeof scenery_error):world_scenery_load_files_streamed(scenery_files.data(),std::size(scenery_files),"models",cam.x,cam.z,scenery_config,scenery_error,sizeof scenery_error);};
    startup_plate.phase="Preparing scenery and character models";startup_plate.draw(true);
    WorldScenery *scenery=load_initial_scenery();
    if(!scenery&&studio.loaded&&!studio.live.assets.empty()){
        std::fprintf(stderr,"[Studio] Saved asset definitions could not load: %s\n",scenery_error);
        studio.message=std::string("Saved asset definitions could not load: ")+scenery_error;studio.live={};studio.bind();std::string ignored;studio.configure(studio.live,ignored);studio.loaded=false;studio.dirty=studio.open=studio.rebuild=true;
        scenery=load_initial_scenery();
        world_environment_free(studio_environment);studio_environment=nullptr;valid=reload_studio_shaders(false,false)&&valid;
    }
    if(!scenery)std::fprintf(stderr,"[World] Authored scenery: %s\n",scenery_error);
    valid=scenery&&valid;
    studio.capture(scenery);
    if(studio.loaded&&(!studio.live.placements.empty()||Studio::source_bound(studio.live)||!studio.live.terrain.empty()||!studio.live.waters.empty())){
        auto *edited=studio.scene(studio.live,studio.message,scenery);
        if(edited&&world_scenery_commit_prepared(edited,scenery)){world_scenery_free(scenery);scenery=edited;}
        else {if(edited)world_scenery_free(edited);std::fprintf(stderr,"[Studio] Saved scene rejected: %s\n",studio.message.c_str());
            // Keep the saved draft available for repair without making the
            // entire project unopenable. Reground the original source safely.
            studio.live={};studio.bind();auto *fallback=studio.scene(studio.live,studio.message,scenery);
            if(fallback&&world_scenery_commit_prepared(fallback,scenery)){world_scenery_free(scenery);scenery=fallback;}else {world_scenery_free(fallback);valid=false;}
            studio.loaded=false;studio.dirty=true;studio.rebuild=true;studio.open=true;
            if(studio_shader_features||studio_environment_features)valid=reload_studio_shaders(false,false)&&valid;
            world_environment_free(studio_environment);studio_environment=nullptr;
        }
    }
    studio.refresh_scene(scenery);valid=landscape_publish_cover()&&valid;
    studio.materials(scenery);studio_castle_culling=!studio.moved_architecture();
    // Resolve the authored shell once. The conservative interior prism stays
    // clear of its walls, door thresholds and sloping roof; flying/outdoor
    // cameras retain the ordinary draw order.
    WorldSceneryPlacement tavern_shell={};bool have_tavern_shell=false,early_opaque_enabled=false;
    auto refresh_tavern_shell=[&](){
        have_tavern_shell=false;
        for(size_t i=0;i<world_scenery_stats(scenery).source_instances;i++){
            WorldSceneryPlacement p;if(world_scenery_placement(scenery,i,&p)&&std::strcmp(p.alias,"village_tavern")==0){tavern_shell=p;have_tavern_shell=true;break;}
        }
        early_opaque_enabled=have_tavern_shell&&std::getenv("VALWOOD_QA_DISABLE_EARLY_OPAQUE")==nullptr;
    };
    refresh_tavern_shell();
    auto inside_tavern_shell=[&](const Camera &camera) {
        if(!early_opaque_enabled)return false;
        const double dx=(camera.x-tavern_shell.x)/tavern_shell.scale,dz=(camera.z-tavern_shell.z)/tavern_shell.scale;
        const double c=std::cos(tavern_shell.yaw),s=std::sin(tavern_shell.yaw);
        const double x=c*dx-s*dz,z=s*dx+c*dz,y=(camera.y-tavern_shell.y)/tavern_shell.scale;
        return std::abs(x)<9.5&&std::abs(z)<7.5&&y>.05&&y<4.2;
    };
    size_t early_opaque_frames=0,late_opaque_skipped_frames=0;
    const bool authored_first_enabled=std::getenv("VALWOOD_QA_DISABLE_AUTHORED_FIRST")==nullptr;
    size_t authored_first_frames=0;
    uint64_t source_camera_casts=0,source_camera_blocks=0,source_camera_query_failures=0;
    std::array<uint64_t,10> destination_wait_frames={};
    std::array<double,10> destination_wait_ms={};
    double destination_probe_ms=0,destination_material_ms=0;
    size_t early_gpu_submitted_frames=0;
    const bool pool_reflection_scissor_enabled=!(launch->frames&&std::getenv("VALWOOD_QA_DISABLE_POOL_REFLECTION_SCISSOR"));
    const bool pool_reflection_clip_enabled=!(launch->frames&&std::getenv("VALWOOD_QA_DISABLE_POOL_REFLECTION_CLIP"));
    uint64_t pool_reflection_clip_tests=0,pool_reflection_clip_outside=0,pool_reflection_clip_skips=0;
    const bool force_legacy_ocean_reflection=launch->frames&&std::getenv("VALWOOD_QA_FORCE_LEGACY_OCEAN_REFLECTION");
    uint64_t ocean_legacy_candidate_frames=0,ocean_disabled_frames=0,local_mirror_consumer_frames=0;
    uint64_t unused_ocean_mirror_skips=0,water_mirror_rendered_frames=0;
    const bool wildlife_preparation_enabled=!(launch->frames&&std::getenv("VALWOOD_QA_DISABLE_WILDLIFE_PREPARATION"));
    const bool river_reflection_scissor_enabled=!(launch->frames&&std::getenv("VALWOOD_QA_DISABLE_RIVER_REFLECTION_SCISSOR"));
    uint64_t river_reflection_support_frames=0,river_reflection_restricted_frames=0,river_reflection_geometry_pixels=0,river_reflection_full_pixels=0;
    size_t pool_reflection_scissor_frames=0;
    uint64_t pool_reflection_geometry_pixels=0,pool_reflection_full_pixels=0;
    valid=landscape.init(scenery,showcase_terrain.side>0)&&valid;
    auto weather_drip_cache=std::unique_ptr<world_weather_drips::Cache>(new(std::nothrow)world_weather_drips::Cache);
    bool weather_drips_prepared=false;double weather_drips_origin_x=cam.x,weather_drips_origin_z=cam.z;
    auto prepare_weather_drips=[&](const Camera &eye){
        if(!weather_drip_cache)return;
        world_weather_drips::prepare(*weather_drip_cache,scenery,"models",eye.x,eye.z,weather_surface,nullptr);
        weather_drips_prepared=true;weather_drips_origin_x=eye.x;weather_drips_origin_z=eye.z;
    };
    if(studio.live.weather_global.enabled||!studio.live.weather_areas.empty())prepare_weather_drips(cam);
    startup_status("authored scenery and landscape materials");
    if(valid&&!studio_surface_gpu.prepare(landscape.masks,studio.live.surfaces,studio.message)){std::fprintf(stderr,"[Studio] Surface library rejected: %s\n",studio.message.c_str());studio.live.surfaces={};studio.dirty=studio.open=true;valid=reload_studio_shaders(studio_shader_features,studio_environment_features,&studio.live)&&valid;}
    char population_error[256]={};
    const bool stock_population_requested=!city_showcase&&!stronghold_finish&&std::getenv("VALWOOD_DISABLE_VILLAGE_POPULATION")==nullptr;
    // Authoring needs the character library even in isolated districts whose
    // stock castle crowds, training host and watch are intentionally disabled.
    const bool population_library_enabled=stock_population_requested||launch->editor||!studio.live.actors.empty();
    WorldVillagePopulation *population=population_library_enabled?world_village_population_load("models",population_error,sizeof(population_error)):nullptr;
    if(population_library_enabled&&!population)std::fprintf(stderr,"[World] Village character library: %s\n",population_error);
    // A custom artist project may not ship this optional character bank. Keep
    // its editor/draft open for repair; a game which requires the bank fails.
    valid=(!population_library_enabled||population||launch->editor)&&valid;
    // If an optional editor bank is unavailable, its dependent stock training,
    // watch and combat models must also stay disabled. Required game banks
    // still fail the validation above instead of silently hiding their crowd.
    const bool population_enabled=stock_population_requested&&population;
    world_village_population_stock_enabled(population,population_enabled);
    studio.population_ptr=population;studio.materials(scenery);
    studio.memory_appearance_init();
    std::string stronghold_error;
    WorldStronghold *stronghold=stronghold_finish?world_stronghold_load("models/stronghold",stronghold_error):nullptr;
    if(stronghold_finish&&!stronghold){std::fprintf(stderr,"[Grandstone] %s\n",stronghold_error.c_str());valid=false;}
    active_stronghold=stronghold;
    struct StructureCookResult {
        std::unique_ptr<world_playtest_walk::Support> support;
        world_structure_collision::Capture source;
        std::string error;std::uint64_t fingerprint=0,triangles=0,vertices=0;
        unsigned partitions=0;double milliseconds=0;bool ok=false;
    };
    WorldStructureGPU structure_gpu;
    const auto *structure_baseline_walk=active_playtest_walk;
    const bool structure_baseline_policy=active_structure_world;
    const auto *structure_baseline_city=active_city_physics;
    const bool structure_source_mode=studio.structure_mode()!=world_editor_scene_guard::Mode::None;
    const auto structure_baseline_documents=!structure_source_mode&&structure_baseline_walk?structure_baseline_walk->documents():std::vector<world_physics::Document>{};
    const auto structure_baseline_bounds=structure_baseline_walk?structure_baseline_walk->bounds():world_playtest_walk::Bounds{};
    const world_structure_runtime::Ground structure_ground=[](double px,double pz,const void*){return double(world_atlas_height(px,pz));};
    std::unique_ptr<world_playtest_walk::Support> structure_support;
    std::future<StructureCookResult> structure_job;
    std::unique_ptr<StructureCookResult> structure_ready;
    auto structure_cache=std::make_shared<world_structure_collision::Cache>();
    const auto structure_main_thread=std::this_thread::get_id();
    std::uint64_t structure_live_fingerprint=0,structure_started_revision=0;
    unsigned structure_async_started=0,structure_async_commits=0,structure_async_failures=0;
    double structure_last_ms=0;
    auto cook_structure=[structure_cache,structure_main_thread,structure_source_mode,structure_baseline_documents,structure_baseline_bounds](world_structure_collision::Capture capture)->StructureCookResult{
        StructureCookResult result;const auto begin=std::chrono::steady_clock::now();result.fingerprint=capture.fingerprint;
        try{
            world_structure_collision::Geometry geometry;
            if(world_structure_collision::build(capture,geometry,result.error,structure_cache.get())){
                if(!structure_source_mode&&!capture.procedural.meshes.empty()&&!structure_baseline_documents.empty()){
                    geometry.documents.insert(geometry.documents.begin(),structure_baseline_documents.begin(),structure_baseline_documents.end());
                    const auto &b=structure_baseline_bounds;geometry.xmin=std::min(geometry.xmin,b.xmin);geometry.xmax=std::max(geometry.xmax,b.xmax);geometry.zmin=std::min(geometry.zmin,b.zmin);geometry.zmax=std::max(geometry.zmax,b.zmax);geometry.ymin=std::min(geometry.ymin,b.ymin);geometry.ymax=std::max(geometry.ymax,b.ymax);
                    for(const auto &d:structure_baseline_documents)for(const auto &b:d.bodies){geometry.vertices+=b.vertices.size();geometry.triangles+=b.triangles.size();}
                }
                result.triangles=geometry.triangles;result.vertices=geometry.vertices;result.partitions=unsigned(geometry.documents.size());
                if(geometry.empty()){
                    if(!structure_source_mode)result.ok=true;
                    else result.error="Structural source collision has no supporting geometry";
                }else{
                    result.support=std::make_unique<world_playtest_walk::Support>();
                    const world_playtest_walk::Bounds bounds={geometry.xmin-.5,geometry.xmax+.5,geometry.zmin-.5,geometry.zmax+.5,geometry.ymin-.5,geometry.ymax+.5};
                    result.ok=result.support->configure(std::move(geometry.documents),bounds,{},result.error,false)&&result.support->handoff_static_query_owner(structure_main_thread,result.error);
                }
            }
        }catch(const std::exception &e){result.error=std::string("Structural collision cook failed: ")+e.what();}
        if(!result.ok)result.support.reset();
        result.source=std::move(capture);result.milliseconds=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();return result;
    };
    if(structure_source_mode||!studio.live.structures.empty()){
        world_structure_collision::Capture capture;std::string error;
        if(world_structure_collision::capture(studio.project_directory,scenery,studio.live,studio.structure_mode(),capture,error,structure_ground,nullptr)){
            auto initial=cook_structure(std::move(capture));
            if(initial.ok&&!structure_gpu.prepare(initial.source.procedural,error))initial.ok=false;
            if(initial.ok){structure_live_fingerprint=initial.fingerprint;structure_support=std::move(initial.support);active_playtest_walk=structure_support?structure_support.get():structure_baseline_walk;active_structure_world=structure_support?true:structure_baseline_policy;active_city_physics=structure_support?nullptr:structure_baseline_city;structure_last_ms=initial.milliseconds;
                std::printf("[Structure Collision] Source scene ready: %llu triangles, %u partitions, %.1fms\n",(unsigned long long)initial.triangles,initial.partitions,initial.milliseconds);
            }else error=initial.error;
        }
        if(!structure_support&&(structure_source_mode||!studio.live.structures.empty())){std::fprintf(stderr,"[Structure Collision] Cannot load matching source collision: %s\n",error.c_str());studio.message=error;valid=false;}
    }
    // Saved NPC paths must see the same imported stronghold collision owner as
    // newly authored paths; do not validate them before this owner is ready.
    {Studio::RuntimePrepared prepared;std::string error;if(studio.prepare_runtime(studio.live,scenery,prepared,error)&&studio.commit_runtime(prepared,scene_particles,torch_flames)){}else{studio.message="Saved runtime edits need repair: "+error;studio.open=studio.dirty=true;studio.live.actors.clear();studio.live.navigation.clear();studio.live.particles.clear();world_village_population_configure(population,nullptr,0);world_scene_particles_set_emitters(scene_particles,scene_emitters.data(),scene_emitters.size());world_torch_flames_set_emitters(torch_flames,flame_emitters.data(),flame_emitters.size());}}
    WorldSnow *snow=stronghold_finish?world_snow_create("shaders","audio/snow_step.wav",stronghold_error):nullptr;
    if(stronghold_finish&&!snow){std::fprintf(stderr,"[Snow] %s\n",stronghold_error.c_str());valid=false;}
    const bool population_frustum_enabled=std::getenv("VALWOOD_QA_DISABLE_POPULATION_FRUSTUM")==nullptr;
    auto draw_population_color=[&](Program &program,const Camera &camera,const M4 &view_projection) {
        if(population_frustum_enabled)
            world_village_population_draw_frustum(population,program.id,camera.x,camera.y,camera.z,view_projection.m);
        else world_village_population_draw(population,program.id,camera.x,camera.y,camera.z);
    };
    char pets_error[256]={};
    const bool pets_enabled=!source_showcase&&std::getenv("VALWOOD_DISABLE_VILLAGE_PETS")==nullptr;
    WorldVillagePets *pets=pets_enabled?world_village_pets_load("models",pets_error,sizeof(pets_error)):nullptr;
    if(pets_enabled&&!pets)std::fprintf(stderr,"[World] Village pets: %s\n",pets_error);
    valid=(!pets_enabled||pets)&&valid;
    startup_status("residents, snow and pets");
    char training_error[256]={};
    WorldCastleTraining *training=population_enabled?world_castle_training_load("models",training_error,sizeof(training_error)):nullptr;
    if(population_enabled&&!training)std::fprintf(stderr,"[World] Castle training: %s\n",training_error);
    valid=(!population_enabled||training)&&valid;
    Camera dialogue_camera;bool dialogue_camera_active=false;
    WorldTrainingCombat fight;TrainingWeapon training_weapon;world_combat_vfx::System combat_vfx;TrainingSounds training_sounds;
    world_blood_effects::System blood_effects;
    const bool blood_enabled=(arena_enabled||first_journey_enabled)&&std::filesystem::is_regular_file("blood_assets/BLOOD_ASSETS.json");
    char blood_error[256]={};
    WorldBloodRenderer *blood_renderer=blood_enabled?world_blood_create("blood_assets",blood_error,sizeof blood_error):nullptr;
    if(blood_enabled){
        const bool ok=blood_renderer&&blood_effects.configure(world_blood_config(blood_renderer))&&world_blood_prepare(blood_renderer,target.w,target.h);
        if(!ok)std::fprintf(stderr,"[Blood] Source playback load failed: %s\n",blood_error);
        valid=ok&&valid;
    }
    unsigned combat_vfx_arrow_hits=0;
    world_combat_feedback::System combat_feedback;
    std::uint64_t combat_feedback_legacy_serial=0,combat_feedback_legacy_attack=0;
    double combat_feedback_clock=-1;
    bool combat_feedback_was_paused=false;
    std::uint64_t combat_feedback_audio_dispatches=0,combat_feedback_haptic_dispatches=0;
    world_arena_combat::Arena arena_game;WorldArenaRender *arena_renderer=nullptr;
    WorldCombatVfxRenderer *combat_vfx_renderer=world_combat_vfx_create();valid=(combat_vfx_renderer!=nullptr)&&valid;
    char motion_error[256]={};
    const char *motion_directory=std::getenv("VALWOOD_QA_COMBAT_DIRECTORY");
    const bool aquatic_scene=!joust_enabled&&city_showcase&&std::filesystem::is_regular_file("models/aquatic_hero/AQUATIC-HERO.json");
    const bool combat_motion_enabled=!joust_enabled&&(population_enabled||playtest_enabled||first_journey_enabled||aquatic_scene);
    const bool character_polish_enabled=(arena_enabled||first_journey_enabled||world_exploration)&&std::filesystem::is_regular_file("CHARACTER_POLISH.json");
    arena_game.keep_defeated=character_polish_enabled;
    // Arena outfits own their exact streamed poses. Keep only a compact setup
    // driver here instead of hundreds of unused legacy body/loadout GPU banks.
    const bool arena_compact_roster_enabled=arena_enabled;
    const bool compact_driver_enabled=battlefield_enabled;
    const char *compact_story_root=std::getenv("VALWOOD_BATTLEFIELD_COMBAT_ROOT");
    if(compact_driver_enabled&&(!compact_story_root||!*compact_story_root)){std::fprintf(stderr,"[Battlefield] Compact source bank is required\n");return 1;}
    const std::filesystem::path compact_root=compact_driver_enabled?compact_story_root:"npc_knights/knight09";
    WorldCombatMotion *combat_motion=combat_motion_enabled?((arena_compact_roster_enabled||compact_driver_enabled)?
        world_combat_motion_load_compact((compact_root/"body").c_str(),(compact_root/"anatomy").c_str(),(compact_root/"poses.pnt").c_str(),1,false,motion_error,sizeof motion_error,512u):
        world_combat_motion_load(motion_directory&&*motion_directory?motion_directory:"models/longsword_mocap",motion_error,sizeof motion_error,character_polish_enabled)):nullptr;
    const bool arena_knight_variants=combat_motion&&world_combat_motion_variant_stats(combat_motion).available_mask==15;
    if(combat_motion_enabled&&!combat_motion)std::fprintf(stderr,"[World] Combat motion: %s\n",motion_error);
    valid=(!combat_motion_enabled||combat_motion)&&valid;
    if(combat_motion&&!arena_compact_roster_enabled&&!compact_driver_enabled&&character_polish_enabled&&std::filesystem::is_directory("models/weapon_loadouts")&&!world_combat_motion_load_loadouts(combat_motion,"models/weapon_loadouts")){std::fprintf(stderr,"[Weapon] Loadout bank failed\n");valid=false;}
    if(combat_motion&&!arena_compact_roster_enabled&&!compact_driver_enabled&&std::filesystem::is_directory("models/unarmed_hero")&&!world_combat_motion_load_unarmed(combat_motion,"models/unarmed_hero")){std::fprintf(stderr,"[Weapon] Unarmed bank failed\n");valid=false;}
    if(combat_motion&&!launch->editor&&!arena_compact_roster_enabled&&!compact_driver_enabled&&std::filesystem::is_directory("models/hero_locomotion")&&!world_combat_motion_load_locomotion(combat_motion,"models/hero_locomotion",motion_error,sizeof motion_error)){std::fprintf(stderr,"[Motion] Directional locomotion: %s\n",motion_error);valid=false;}
    if(combat_motion&&!launch->editor&&!arena_compact_roster_enabled&&!compact_driver_enabled&&std::filesystem::is_regular_file("models/hero_grips/arm_grips.bin")&&!world_combat_motion_load_grips(combat_motion,"models/hero_grips/arm_grips.bin",motion_error,sizeof motion_error)){std::fprintf(stderr,"[Motion] Arm grip stream: %s\n",motion_error);valid=false;}
    if(combat_motion&&!arena_compact_roster_enabled&&!compact_driver_enabled&&!arena_knight_variants&&character_polish_enabled&&std::filesystem::is_directory("models/fatality_hero")&&!world_combat_motion_load_fatalities(combat_motion,"models/fatality_hero")){std::fprintf(stderr,"[Combat] Fatality bank failed\n");valid=false;}
    const bool combat_anatomy_enabled=character_polish_enabled&&std::filesystem::is_directory("models/combat_anatomy");
    const bool combat_anatomy_ready=combat_anatomy_enabled&&((arena_compact_roster_enabled||compact_driver_enabled)?world_combat_motion_anatomy_stats(combat_motion).loaded:world_combat_motion_load_anatomy(combat_motion,"models/combat_anatomy"));
    if(combat_anatomy_enabled&&!combat_anatomy_ready){std::fprintf(stderr,"[Combat] Anatomical contact / severing bank failed\n");valid=false;}
    if(arena_knight_variants&&!combat_anatomy_ready){std::fprintf(stderr,"[Combat] Knight variants require their matching anatomy bank\n");valid=false;}
    // Each assigned appearance owns its source geometry, sockets and anatomy.
    WorldCombatMotion *arena_orc_motion=nullptr;
    bool arena_orc_assets_ready=false;
    bool arena_orc_original_textures_ready=false;
    world_orc_roster::Roster arena_orc_roster;
    const bool arena_orc_roster_present=arena_enabled&&std::filesystem::is_regular_file("arena_orcs_roster/ORC_ROSTER.cfg");
    bool arena_orc_catalog_ready=false;
    std::string arena_orc_catalog_error,arena_orc_character_loaded;
    int arena_orc_character_draft=0;
    unsigned arena_orc_resident_banks=0,arena_orc_peak_banks=0,arena_orc_bank_loads=0;
    if(arena_orc_roster_present){
        std::error_code error;
        const auto bytes=std::filesystem::file_size("arena_orcs_roster/ORC_ROSTER.cfg",error);
        std::ifstream input("arena_orcs_roster/ORC_ROSTER.cfg");
        arena_orc_catalog_ready=!error&&bytes<=65536&&world_orc_roster::read(input,arena_orc_roster,arena_orc_catalog_error);
        if(arena_orc_catalog_ready){
            arena_orc_character_draft=arena_orc_roster.index(arena_orc_roster.default_id);
            std::ifstream selection("arena_orc_character.cfg");std::string magic,id,end,trailing;unsigned version=0;
            if(selection>>magic>>version>>id>>end&&magic=="VALWOOD_ORC_CHARACTER"&&version==1&&end=="end"&&
               !(selection>>trailing)&&arena_orc_roster.index(id)>=0)arena_orc_character_draft=arena_orc_roster.index(id);
        }else std::fprintf(stderr,"[Arena Orcs] Roster unavailable: %s\n",arena_orc_catalog_error.c_str());
    }
    auto arena_orc_paths=[&](const std::string &id,std::string &body,std::string &anatomy,std::string &materials,std::string &error){
        if(arena_orc_catalog_ready){
            const int index=arena_orc_roster.index(id);
            if(index<0){error="The selected Orc character is not in this roster.";return false;}
            const auto &entry=arena_orc_roster.entries[unsigned(index)];
            body="arena_orcs_roster/"+entry.body_directory;anatomy="arena_orcs_roster/"+entry.anatomy_directory;materials="arena_orcs_roster/"+entry.materials_config;
        }else if(!arena_orc_roster_present&&id=="gnur"){
            body="arena_orcs/body";anatomy="arena_orcs/anatomy";materials="arena_orcs/ORC_MATERIALS.cfg";
        }else{error="The Orc character roster could not be loaded.";return false;}
        if(!std::filesystem::is_regular_file(body+"/combat_body_000.glb")||
           !std::filesystem::is_regular_file(body+"/combat_body_171.glb")||
           !std::filesystem::is_regular_file(body+"/combat_weapon_pivots.bin")||
           !std::filesystem::is_regular_file(anatomy+"/anatomy.bin")){
            error="The selected Orc character is missing combat assets.";return false;
        }
        return true;
    };
    auto arena_orc_load=[&](const std::string &id,std::string &error)->WorldCombatMotion*{
        std::string body,anatomy,materials;
        if(!arena_orc_paths(id,body,anatomy,materials,error))return nullptr;
        std::ifstream material_status(materials);std::string magic,key,end,trailing;unsigned version=0,original=0;
        if(!(material_status>>magic>>version>>key>>original>>end)||magic!="VALWOOD_ORC_MATERIALS"||version!=1||
           key!="original_textures"||original!=1||end!="end"||(material_status>>trailing)){
            error="The selected Orc needs its original material maps before it can enter battle.";return nullptr;
        }
        char detail[256]={};auto *bank=world_combat_motion_load_opponents(body.c_str(),anatomy.c_str(),detail,sizeof detail);
        if(!world_combat_motion_opponents_ready(bank)){
            world_combat_motion_free(bank);error=std::string("Orc character could not load: ")+detail;return nullptr;
        }
        ++arena_orc_resident_banks;++arena_orc_bank_loads;arena_orc_peak_banks=std::max(arena_orc_peak_banks,arena_orc_resident_banks);
        arena_orc_original_textures_ready=true;
        std::printf("[Arena Orc character] loaded=%s resident=%u peak=%u loads=%u\n",id.c_str(),arena_orc_resident_banks,arena_orc_peak_banks,arena_orc_bank_loads);
        return bank;
    };
    auto arena_orc_release=[&](WorldCombatMotion *bank){
        world_combat_motion_free(bank);if(arena_orc_resident_banks)--arena_orc_resident_banks;
        arena_orc_original_textures_ready=false;
    };
    if(arena_enabled&&!arena_compact_roster_enabled&&(arena_orc_catalog_ready||(!arena_orc_roster_present&&std::filesystem::is_regular_file("arena_orcs/ORC_ASSETS.json")))){
        const std::string id=arena_orc_catalog_ready?arena_orc_roster.entries[unsigned(arena_orc_character_draft)].id:"gnur";
        arena_orc_motion=arena_orc_load(id,arena_orc_catalog_error);
        arena_orc_assets_ready=world_combat_motion_opponents_ready(arena_orc_motion);
        if(arena_orc_assets_ready)arena_orc_character_loaded=id;
        else std::fprintf(stderr,"[Arena Orcs] Purchased opponent bank unavailable: %s\n",arena_orc_catalog_error.c_str());
    }
    if(arena_compact_roster_enabled){
        arena_orc_assets_ready=arena_orc_catalog_ready;
        for(const auto &entry:arena_orc_roster.entries)arena_orc_assets_ready=arena_orc_assets_ready&&std::filesystem::is_regular_file("compact_orcs/"+entry.id+"/poses.pnt");
        arena_orc_original_textures_ready=arena_orc_assets_ready;
        world_combat_motion_set_actor_mask(combat_motion,1);
    }
    // Story foes use the same purchased knight geometry and exact compact
    // source pose/anatomy packet as the arena. Keep the legacy player driver
    // resident for supported equipment; its enemy actor is explicitly hidden.
    WorldCombatMotion *battlefield_combat_motion=nullptr;
    if(battlefield_enabled){
        const char *root=std::getenv("VALWOOD_BATTLEFIELD_COMBAT_ROOT");
        if(!root||!*root){std::fprintf(stderr,"[Battlefield] Knight combat bank is required\n");return 1;}
        const auto directory=std::filesystem::path(root);
        battlefield_combat_motion=world_combat_motion_load_compact((directory/"body").c_str(),(directory/"anatomy").c_str(),(directory/"poses.pnt").c_str(),30,false,motion_error,sizeof motion_error,0u);
        if(!battlefield_combat_motion){std::fprintf(stderr,"[Battlefield] Knight bank: %s\n",motion_error);return 1;}
        if(!world_combat_motion_prepare_crowd(battlefield_combat_motion)){std::fprintf(stderr,"[Battlefield] Could not prepare persistent fallen soldiers\n");return 1;}
        world_combat_motion_set_actor_mask(combat_motion,1);
    }
    #include "world_arena_appearance_runtime.inc"
    auto arena_opponent_motion=[&](unsigned id=world_combat_capacity::npcs)->WorldCombatMotion*{
        if(battlefield_combat_motion)return battlefield_combat_motion;
        if(arena_compact_roster_enabled&&!arena_enemy_appearances.empty()){
            if(id<arena_actor_banks.size()&&arena_actor_banks[id])return arena_actor_banks[id];
            id=unsigned(arena_game.current_index());
            if(id>=arena_enemy_appearances.size())id=0;
            return arena_actor_banks[id];
        }
        return arena_enabled&&arena_game.config().opponents==world_arena_opponents::Kind::Orc?arena_orc_motion:combat_motion;
    };
    auto arena_player_motion=[&](){return arena_player_bank?arena_player_bank:combat_motion;};
    auto arena_opponent_anatomy_ready=[&](unsigned id=world_combat_capacity::npcs){
        if(battlefield_combat_motion)return world_combat_motion_anatomy_stats(battlefield_combat_motion).loaded;
        if(arena_compact_roster_enabled&&!arena_enemy_appearances.empty())return world_combat_motion_anatomy_stats(arena_opponent_motion(id)).loaded;
        return arena_enabled&&arena_game.config().opponents==world_arena_opponents::Kind::Orc?arena_orc_assets_ready:combat_anatomy_ready;
    };
    bool fatality_flat_arena=arena_enabled;
    world_combat_motion_fatality_ground(combat_motion,[](double x,double z,void *arena){return *static_cast<const bool*>(arena)?64.:double(world_atlas_walk_height(x,z));},&fatality_flat_arena);
    world_combat_motion_fatality_ground(battlefield_combat_motion,[](double x,double z,void*){return double(world_atlas_walk_height(x,z));},nullptr);
    world_combat_motion_fatality_ground(arena_orc_motion,[](double x,double z,void *arena){return *static_cast<const bool*>(arena)?64.:double(world_atlas_walk_height(x,z));},&fatality_flat_arena);
    auto arena_select_orc_character=[&](std::string &error){
        if(arena_compact_roster_enabled)return arena_orc_assets_ready;
        if(!arena_orc_catalog_ready)return arena_orc_assets_ready;
        if(arena_orc_character_draft<0||unsigned(arena_orc_character_draft)>=arena_orc_roster.entries.size()){
            error="Choose a valid Orc character.";return false;
        }
        const auto &entry=arena_orc_roster.entries[unsigned(arena_orc_character_draft)];
        std::string body,anatomy,materials;
        // Missing paths never release the last valid bank. Deeper loader
        // failure restores it through the same bounded one-bank boundary.
        if(!arena_orc_paths(entry.id,body,anatomy,materials,error))return false;
        const bool loaded=world_orc_roster::switch_bank(arena_orc_motion,arena_orc_character_loaded,entry.id,arena_orc_load,arena_orc_release,error);
        arena_orc_assets_ready=world_combat_motion_opponents_ready(arena_orc_motion);
        world_combat_motion_fatality_ground(arena_orc_motion,[](double x,double z,void *arena){return *static_cast<const bool*>(arena)?64.:double(world_atlas_walk_height(x,z));},&fatality_flat_arena);
        return loaded&&arena_orc_assets_ready;
    };
    if(aquatic_scene&&!compact_driver_enabled){const bool loaded=world_combat_motion_load_aquatics(combat_motion,"models/aquatic_hero",motion_error,sizeof motion_error);if(!loaded)std::fprintf(stderr,"[Aquatics] Hero bank: %s\n",motion_error);valid=loaded&&valid;}
    if(combat_motion_enabled&&!launch->editor){const bool ready=training_weapon.init();if(!ready)std::fprintf(stderr,"[World] Combat view shader initialization failed\n");valid=ready&&valid;}
    world_neck_blood::System neck_blood;
    world_sever_blood::System sever_blood;
    world_aquatic_effects::System aquatic_system;
    world_aquatic_effects::Renderer aquatic_renderer;
    glUseProgram(0);glBindVertexArray(0);glBindBuffer(GL_ARRAY_BUFFER,0);
    const bool aquatic_renderer_ready=aquatic_scene&&aquatic_renderer.init();
    if(aquatic_scene&&!aquatic_renderer_ready){std::fprintf(stderr,"[Aquatics] Renderer initialization failed\n");valid=false;}
    auto aquatic_habitat=[](double x,double z,double &surface,double &bed,void*){
        float water=0,depth=0;if(!world_atlas_local_water(x,z,&water,&depth))return false;
        surface=water;bed=world_atlas_height(x,z);return std::isfinite(bed)&&surface>bed;
    };
    auto aquatic_frame=[](const Camera &eye,const Lighting &light,const M4 &matrix){
        world_aquatic_effects::Frame out;std::copy_n(matrix.m,16,out.vp);out.eye[0]=eye.x;out.eye[1]=eye.y;out.eye[2]=eye.z;
        const auto right=eye.right(),up=norm(cross(right,eye.forward()));
        auto copy=[](float *to,V3 v){to[0]=v.x;to[1]=v.y;to[2]=v.z;};
        copy(out.right,right);copy(out.up,up);copy(out.sun,light.sun);copy(out.sun_color,light.sunColor);copy(out.ambient,light.skyLight);return out;
    };
    WorldJoustRender joust_renderer;WorldJoustImpacts joust_splinters;
    if(joust_enabled&&!joust_splinters.load("joust-assets/wood_chip.glb")){std::fprintf(stderr,"[Joust] Splinter asset failed\n");valid=false;}
    if(joust_enabled){std::string error;if(!joust_renderer.load("joust-assets",error)){std::fprintf(stderr,"[Joust] Assets failed: %s\n",error.c_str());valid=false;}}
    if(arena_enabled){
        char error[256]={};arena_renderer=world_arena_render_create({arena_origin_x,arena_floor+.02,arena_origin_z,12,12},error,sizeof error);
        const bool ok=arena_renderer&&world_combat_motion_prepare_crowd(combat_motion)&&world_arena_render_load_bow(arena_renderer,"arena_assets",arena_bow_sockets,error,sizeof error);
        if(!ok)std::fprintf(stderr,"[Arena] Load failed: %s\n",error);valid=ok&&valid;
    }
    if(first_journey_enabled&&!launch->editor)valid=character_polish_enabled&&blood_enabled&&world_combat_motion_prepare_crowd(combat_motion)&&valid;
    const bool qa_training=std::getenv("VALWOOD_QA_TRAINING_COMBAT")!=nullptr;
    const bool qa_training_controls=qa_training&&!std::strcmp(std::getenv("VALWOOD_QA_TRAINING_COMBAT"),"controls");
    unsigned qa_training_events=0,fight_starts=0,fight_paused_frames=0;uint64_t fight_begin_frame=0,fight_end_frame=0;
    uint64_t playtest_hero_draw_frames=0,playtest_enemy_visible_frames=0;
    std::array<std::array<std::uint64_t,2>,2> combat_timing_windows={};std::array<unsigned,2> combat_timing_warmup={};
    float fight_mouse_x=0,fight_mouse_y=0;double fight_capture_max_ms=0;bool fight_capture_focused=false;
    char watch_error[256]={};
    WorldVillageWatch *watch=population_enabled?world_village_watch_load("models",watch_error,sizeof(watch_error)):nullptr;
    if(population_enabled&&!watch)std::fprintf(stderr,"[World] Shield watch: %s\n",watch_error);
    valid=(!population_enabled||watch)&&valid;
    WorldWildlife *wildlife=world_wildlife_create("models");
    // The arrival canopy has a small dedicated bank even in the city showcase.
    auto desired_wildlife=[&](bool loading){
        const int habitat=loading?world_wildlife_loading_desired(wildlife,cam.x,cam.z):world_wildlife_desired(wildlife,cam.x,cam.z);
        return city_showcase&&habitat!=3?-1:habitat;
    };
    if(!wildlife)std::fprintf(stderr,"[World] Wildlife model bank unavailable\n");
    if(wildlife&&access("jungle_expansion.json",F_OK)==0)valid=world_wildlife_expand_jungle(wildlife)&&valid;
    valid=wildlife&&valid;
    size_t wildlife_peak_visible=0;
    int wildlife_pending=-2;
    WorldBanditCamp *bandits=world_bandit_camp_create("models");
    if(!bandits)std::fprintf(stderr,"[World] Bandit model bank unavailable\n");
    valid=bandits&&valid;
    size_t bandit_peak_visible=0;
    int bandit_pending=-2;
    WorldTrailCamp *arrival_camp=world_trail_camp_create("models");
    if(!arrival_camp)std::fprintf(stderr,"[World] Arrival-camp model bank unavailable\n");
    WorldTrailCamp *first_journey_townsfolk=nullptr;
    bool arrival_camp_enabled=false;int arrival_camp_pending=-1;char arrival_camp_error[256]={};
    valid=arrival_camp&&valid;
    WorldTrailCamp *trail_camp=world_trail_camp_create("models");
    if(!trail_camp)std::fprintf(stderr,"[World] Trail-camp model bank unavailable\n");
    valid=trail_camp&&valid;int trail_camp_pending=-1;char trail_camp_error[256]={};
    startup_status("combat and wildlife systems");
    const bool dragons_enabled=!city_showcase&&std::getenv("VALWOOD_QA_DISABLE_CASTLE_DRAGONS")==nullptr;
    WorldCastleDragons *dragons=dragons_enabled?world_castle_dragons_create():nullptr;
    WorldDragonBreath *dragon_breath=nullptr;
    int dragons_pending=-1;char dragons_error[256]={};
    valid=(!dragons_enabled||dragons)&&valid;
    auto commit_dragons=[&](bool active)->bool {
        if(!world_castle_dragons_commit(dragons,active,dragons_error,sizeof dragons_error))return false;
        if(!active){world_dragon_breath_free(dragon_breath);dragon_breath=nullptr;return true;}
        if(!dragon_breath){
            dragon_breath=world_dragon_breath_create();
            if(!dragon_breath||!world_dragon_breath_load_fire(dragon_breath,"particles/purchased_fire.vfx")||
               !world_dragon_breath_load_smoke(dragon_breath,"particles/purchased_smoke.vfx")){
                std::snprintf(dragons_error,sizeof dragons_error,"Dragon breath art/shader loading failed");return false;
            }
        }
        return true;
    };
    float dragon_time_offset=0;
    if(launch->frames)if(const char *value=std::getenv("VALWOOD_QA_DRAGON_TIME_OFFSET")){
        char *end=nullptr;double t=std::strtod(value,&end);
        if(end&&!*end&&std::isfinite(t)&&std::abs(t)<=100000)dragon_time_offset=float(t);
    }
    unsigned dragon_stage_mask=0;
    auto update_dragon_emitters=[&](float time)->bool {
        world_castle_dragons_update(dragons,double(time)+dragon_time_offset);
        if(!dragon_breath)return true;
        std::array<WorldDragonBreathEmitter,2> emitters={};std::size_t count=0;
        for(unsigned i=0;i<2;i++){
            WorldCastleDragon actor;
            if(!world_castle_dragons_actor(dragons,i,&actor))continue;
            dragon_stage_mask|=1u<<unsigned(actor.stage);
            if(actor.breath<=0)continue;
            auto &e=emitters[count++];std::copy(actor.mouth,actor.mouth+3,e.position);
            std::copy(actor.direction,actor.direction+3,e.direction);e.strength=actor.breath;
            e.variant=i?WorldDragonBreathVariant::Frost:WorldDragonBreathVariant::Fire;
            e.length=18;e.radius=1.8f;e.seed=917u+i*337u;
        }
        return world_dragon_breath_set_emitters(dragon_breath,emitters.data(),count);
    };
    auto dragon_breath_frame=[&](const Camera &eye,const Lighting &light,V3 right,V3 up,V3 forward,const M4 &vp,
            unsigned color,unsigned depth,int width,int height,float time,float wx,float wz){
        WorldDragonBreathFrame f;f.near_plane=view_near_plane();f.tan_half_fov=std::tan(eye.fov*.5f);f.color=color;f.depth=depth;f.width=width;f.height=height;
        std::memcpy(f.vp,vp.m,sizeof f.vp);f.camera[0]=eye.x;f.camera[1]=eye.y;f.camera[2]=eye.z;
        f.time=double(time)+dragon_time_offset;
        auto copy=[](float *out,V3 v){out[0]=v.x;out[1]=v.y;out[2]=v.z;};
        copy(f.right,right);copy(f.up,up);copy(f.forward,forward);copy(f.fog,light.fog);
        f.haze=std::clamp(light.haze,0.f,1.f);f.wind_xz[0]=wx;f.wind_xz[1]=wz;return f;
    };
    std::uint64_t dragon_breath_frames=0,dragon_breath_failures=0;
    unsigned dragon_peak_visible=0,dragon_peak_particles=0,dragon_peak_breathing=0;
    WorldDragonBreathStats dragon_breath_accumulated={};
    const TextureCacheStats loaded_texture_state=texture_cache_stats();
    startup_status("dragon systems");
    std::printf("[World] Shared texture cache: %zu entries, %.2f MiB encoded source, %zu unmanaged uploads\n",
        loaded_texture_state.entries,loaded_texture_state.source_bytes/1048576.,loaded_texture_state.unmanaged_uploads);
    size_t watch_peak_visible=0;
    size_t training_peak_visible=0;
    size_t population_peak_visible=0;
    size_t pets_peak_visible=0;
    // Private capture documents only: inspect real committed editor features
    // before startup streaming/warming, without touching the player's save.
    if(launch->frames&&studio.path.rfind("/tmp/",0)==0){if(const char *tool=std::getenv("VALWOOD_QA_PRODUCTION_TOOL")){char *end=nullptr;const long value=std::strtol(tool,&end,10);if(end&&!*end&&value>=14&&value<=25){studio.open=true;studio.requested_tab=int(value);if(value==25)studio.sound_open();}}}
    if(launch->frames&&studio.path.rfind("/tmp/",0)==0){if(const char *view=std::getenv("VALWOOD_QA_RUNTIME_VIEW")){
        double tx=0,tz=0;float ty=0;bool found=false;
        if(!std::strcmp(view,"audio")&&!studio.live.sounds.empty()){const auto &s=studio.live.sounds.front();tx=s.x;tz=s.z;ty=float(s.y);studio.sound_open();studio.sound_select(0);found=true;}
        if(!std::strcmp(view,"water")){for(unsigned i=0;i<studio.live.waters.size();i++){const auto &w=studio.live.waters[i];if(w.source!=3)continue;tx=w.x;tz=w.z;ty=float(w.level);studio.water_selection=int(i);studio.requested_tab=12;found=true;break;}}
        if(!std::strcmp(view,"npc")&&!studio.live.actors.empty()){const auto &a=studio.live.actors[0];tx=a.points[0][0];tz=a.points[0][1];ty=world_atlas_walk_height(tx,tz)+.8f;studio.actor_selection=0;studio.area_selection=0;studio.requested_tab=10;if(!studio.live.navigation.empty())world_navigation::bake(studio.live.navigation[0],studio.nav_query(scenery),studio.navigation_preview,studio.message);found=true;}
        if(!std::strcmp(view,"particles")&&!studio.live.particles.empty()){const auto &p=studio.live.particles.back();tx=p.x;tz=p.z;ty=float(p.y+.7);studio.particle_selection=int(studio.live.particles.size()-1);studio.requested_tab=11;found=true;}
        if(found){cam.x=tx+9;cam.z=tz+10;cam.y=std::max(double(world_atlas_height(cam.x,cam.z))+2.5,double(ty)+4);cam.yaw=std::atan2(-9.f,10.f);cam.pitch=std::atan2(float(ty-cam.y),std::hypot(9.f,10.f));cam.fly=true;cam.grounded=false;studio.open=!std::getenv("VALWOOD_QA_RUNTIME_CLEAN");third_person=false;current_name="Runtime editor inspection";}
    }}
    if(combat_motion){world_combat_motion_update(combat_motion,fight,cam.x,cam.y-1.75,cam.z,3.14159265359f-cam.yaw,0,false);if(!launch->editor)valid=training_weapon.prepare(combat_motion,target.w,target.h)&&valid;}
    if(dragons_enabled&&!commit_dragons(world_castle_dragons_desired(dragons,cam.x,cam.z))){
        std::fprintf(stderr,"[World] %s\n",dragons_error);valid=false;
    }
    if(!update_dragon_emitters(17.f))valid=false;
    char wildlife_error[256]={};
    if(wildlife&&!world_wildlife_commit(wildlife,desired_wildlife(true),wildlife_error,sizeof(wildlife_error))) {
        std::fprintf(stderr,"[World] %s\n",wildlife_error);valid=false;
    }
    char bandit_error[256]={};
    if(bandits&&!world_bandit_camp_commit(bandits,!city_showcase&&world_bandit_camp_desired(bandits,cam.x,cam.z)==0,bandit_error,sizeof(bandit_error))) {
        std::fprintf(stderr,"[World] Bandit camp: %s\n",bandit_error);valid=false;
    }
    StartingWalkRoute starting_route;
    if(!source_review.active&&!playtest_enabled){
    if(launch->benchmark&&!std::strcmp(launch->scenic,"start")&&!std::strcmp(launch->view,"sweep"))starting_route.begin(cam);
    if(launch->benchmark&&!std::strcmp(launch->scenic,"rainforest-camp")&&!std::strcmp(launch->view,"river"))
        valid=starting_route.begin_trail_camp(cam)&&valid;
    if(launch->benchmark&&!std::strcmp(launch->scenic,"rainforest")&&!std::strcmp(launch->view,"river"))
        valid=starting_route.begin_rainforest(cam)&&valid;
    if(launch->benchmark&&!std::strcmp(launch->scenic,"castle-village")&&!std::strcmp(launch->view,"river"))
        valid=starting_route.begin_village(cam)&&valid;
    if(launch->benchmark&&!std::strcmp(launch->scenic,"castle-demo")&&!std::strcmp(launch->view,"river"))
        valid=starting_route.begin_demo(cam)&&valid;
    if(launch->benchmark&&!std::strcmp(launch->scenic,"castle-hero-entry")&&!std::strcmp(launch->view,"river"))
        valid=starting_route.begin_hero(cam)&&valid;
    if(launch->benchmark&&!std::strcmp(launch->scenic,"castle-drawbridge")&&!std::strcmp(launch->view,"river"))
        valid=starting_route.begin_expansion(cam)&&valid;
    if(launch->benchmark&&!std::strcmp(launch->scenic,"castle-farms")&&!std::strcmp(launch->view,"river"))
        valid=starting_route.begin_farms(cam)&&valid;
    if(launch->benchmark&&!std::strcmp(launch->scenic,"bandit-camp")&&!std::strcmp(launch->view,"river"))
        valid=starting_route.begin_bandits(cam)&&valid;
    if(launch->benchmark&&!std::strcmp(launch->scenic,"castle-west-street")&&!std::strcmp(launch->view,"river"))
        valid=starting_route.begin_streets(cam)&&valid;
    if(launch->benchmark&&!std::strcmp(launch->scenic,"castle-finished-arcade")&&!std::strcmp(launch->view,"river"))
        valid=starting_route.begin_finished_arcade(cam)&&valid;
    if(launch->benchmark&&!std::strcmp(launch->view,"river")&&(!std::strcmp(launch->scenic,"castle-milestone")||!std::strcmp(launch->scenic,"rainforest-milestone")))
        valid=starting_route.begin_milestone(cam,!std::strcmp(launch->scenic,"castle-milestone")?1:2)&&valid;
    if(launch->benchmark&&launch->frames&&std::getenv("VALWOOD_QA_JUNGLE_EXPANSION")&&access("jungle_expansion.json",F_OK)==0)valid=starting_route.begin_jungle_expansion(cam)&&valid;
    if(launch->benchmark&&stronghold_finish&&!std::strcmp(launch->view,"river"))valid=starting_route.begin_stronghold(cam)&&valid;
    }
    terrain.update(cam.x, cam.z, 8);
    // Warm both horizon coverage and the local collision surface before the first
    // walking frame. Later landmark travel keeps the previous image during staging.
    Uint64 warm_begin = SDL_GetPerformanceCounter();
    for (int n = 0; n < 500 && !terrain.view_ready(cam.x, cam.z, cam.fly); n++) {
        terrain.update(cam.x, cam.z, 8);
        SDL_PumpEvents();
        SDL_Delay(1);
    }

    // Publish the complete initial foliage fields before revealing the view.
    for(int i=0;i<96;i++) {
        dressing.update(cam,1./60.,true);
        WorldCoverMetrics cover_metrics={};world_cover_metrics(dressing.cover,&cover_metrics);
        if(!cover_metrics.near_building&&!cover_metrics.tree_building) break;
    }
    horizon.update(cam,400000);
    world_scenery_update(scenery,cam.x,cam.y,cam.z,cam.yaw,cam.pitch,cam.aspect,std::tan(cam.fov*.5f),foliage_detail_scale,foliage_density_scale);
    world_village_population_update(population,cam.x,cam.y,cam.z,17.f,0.f);
    world_castle_training_update(training,cam.x,cam.y,cam.z,17.f,0.f);
    world_village_watch_update(watch,cam.x,cam.y,cam.z,17.f,0.f);
    size_t watch_safe_stations=0;
    for(size_t i=0;i<world_village_watch_stats(watch).actors;i++){
        WorldVillageWatchActor a;
        const bool clear=world_village_watch_actor(watch,i,&a)&&
            std::abs(world_atlas_height(a.x,a.z)-a.y)<.02&&
            world_scenery_can_move(scenery,a.x,a.z,a.x,a.z,a.y,1.85f,.70f);
        if(clear)watch_safe_stations++;
        else {valid=false;std::fprintf(stderr,"[World] Shield-watch station%zu has unsafe support/clearance\n",i);}
    }
    world_village_pets_update(pets,cam.x,cam.y,cam.z,17.f,0.f);
    world_wildlife_update(wildlife,scenery,17.f);
    world_bandit_camp_update(bandits,scenery,17.f);
    if(!world_trail_camp_commit(trail_camp,population_enabled&&world_trail_camp_desired(trail_camp,cam.x,cam.z),trail_camp_error,sizeof(trail_camp_error))){valid=false;launch->exit_code=1;std::fprintf(stderr,"[World] %s\n",trail_camp_error);}
    world_trail_camp_update(trail_camp,scenery,17.f);
    // Build initial terrain support before the first visible rain/splash frame.
    // Teleports still stage incrementally; startup is outside measured play.
    WorldWeatherFrame weather_warm;weather_warm.near_plane=view_near_plane();weather_warm.tan_half_fov=std::tan(cam.fov*.5f);weather_warm.camera[0]=cam.x;weather_warm.camera[1]=cam.y;weather_warm.camera[2]=cam.z;
    weather_warm.intensity=weather_disabled?0.f:stronghold_finish?.40f:.48f*world_atlas_rainforest_weight(cam.x,cam.z);
    weather_warm.fog_strength=weather_disabled?0.f:.62f*std::max(world_rainforest_atmosphere(.28f,world_atlas_rainforest_weight(cam.x,cam.z),visual_surface_finish).mist_weight,
        .40f*world_atlas_village_weight(cam.x,cam.z));
    weather_warm.surface_height=weather_surface;
    weather_warm.cover_height=LandscapeMaterials::cover_height;weather_warm.cover_userdata=&landscape;
    weather_warm.contact_surface=LandscapeMaterials::rain_contact;weather_warm.contact_userdata=&landscape;
    for(int i=0;i<9;i++)world_weather_update(weather,weather_warm);
    std::printf("[World] Atlas 100.8 x 56.7 km; walk %.2f m/s; startup %.1f ms; warm %.1f ms\n", world_atlas_walk_speed(),
        millis(startup), millis(warm_begin));
    if (!valid) {
        launch->exit_code = 1;
        std::fprintf(stderr, "[World] A required renderer resource failed to initialize\n");
    }

    // QA isolation keeps frame/CPU CSV and ordinary rendering intact while
    // omitting the detailed per-stage GPU query rings. Adaptive quality keeps
    // its own independent timing path and is deliberately unchanged.
    const char *frame_times_only=std::getenv("VALWOOD_QA_FRAME_TIMES_ONLY");
    const bool profile_gpu=launch->profile[0]&&!(frame_times_only&&std::strcmp(frame_times_only,"1")==0);
    AdaptiveGPUQueries adaptive_queries;adaptive_queries.init();
    WorldQueries shadow_queries,foliage_queries,early_opaque_queries,dragon_queries;
    WorldQueries queries,reflection_queries,sky_queries,sky_reflection_queries,occlusion_queries,weather_queries;
    WorldQueries terrain_queries,water_queries,temporal_queries,post_queries;
    double sky_gpu_sum=0,occlusion_gpu_sum=0,weather_gpu_sum=0,dragon_gpu_sum=0;size_t lighting_gpu_samples=0;
    double terrain_gpu_sum=0,water_gpu_sum=0,temporal_gpu_sum=0,post_gpu_sum=0;
    if (profile_gpu) {
        dragon_queries.init();shadow_queries.init();foliage_queries.init();early_opaque_queries.init();queries.init();reflection_queries.init();sky_queries.init();sky_reflection_queries.init();occlusion_queries.init();weather_queries.init();
        terrain_queries.init();water_queries.init();temporal_queries.init();post_queries.init();
    }
    bool running = valid, show_map = false, look_active = false, show_tools = !launch->no_hud&&!launch->editor&&!(minimap_valid&&minimap_config.enabled), destination_loading = false;
    bool editor_preview_preparing=false,editor_apply_reuse_materials=false;
    const bool qa_quickspot_stream=launch->editor&&launch->frames&&std::getenv("VALWOOD_QA_QUICKSPOT_STREAM")!=nullptr;
    unsigned qa_quickspot_step=0;bool qa_quickspot_waiting=false;
    std::chrono::steady_clock::time_point qa_quickspot_started;
    const bool qa_terrain_apply=launch->editor&&launch->frames&&std::getenv("VALWOOD_QA_TERRAIN_APPLY_SPEED")!=nullptr&&
        std::getenv("VALWOOD_QA_STUDIO_DOCUMENT")!=nullptr;
    unsigned qa_terrain_apply_phase=0,qa_terrain_apply_before=0;
    std::chrono::steady_clock::time_point qa_terrain_apply_started;
    constexpr std::size_t editor_apply_perf_limit=4096;
    std::deque<std::array<double,5>> editor_apply_perf;
    std::uint64_t editor_apply_perf_total=0;
    std::chrono::steady_clock::time_point editor_apply_wait_started;
    world_designer_viewport::CameraOwnership<Camera> designer_cameras;
    auto sync_designer_camera=[&](){
        if(designer_cameras.sync(studio.open,cam)){
            if(studio.open){cam.fly=true;cam.vy=0;cam.grounded=false;}
            temporal_cut=true;look_active=false;SDL_SetRelativeMouseMode(SDL_FALSE);
        }
    };
    float hour = 17.05f, cloud = .28f, rain = .48f, wind = .85f, mist = .62f, fps = 60;
    float bearing=0,sun_strength=1.12f,sky_fill=1.f,exposure=1.07f,grade_strength=.50f;
    // Opening the authored settlement uses its afternoon lighting look. The
    // higher southern sun can reach the street between tall roofs; every
    // surface, cloud, volumetric sample and shadow uses this same source.
    // These remain the ordinary editable lighting controls during play.
    WorldMarketHeroLook hero_look;
    if(world_market_hero_look_build(world_atlas_village_weight(cam.x,cam.z),hero_look)) {
        hour=hero_look.hour;bearing=hero_look.bearing_degrees;sun_strength=hero_look.sun_strength;
        sky_fill=hero_look.sky_fill;exposure=hero_look.exposure;grade_strength=hero_look.grade_strength;
    }
    // An optional launch preset opens the same editable controls at dusk.
    if(const char *look=std::getenv("VALWOOD_VILLAGE_LIGHTING")) {
        if(!std::strcmp(look,"dusk")) {hour=17.8f;sun_strength=.30f;sky_fill=1.2f;exposure=1.28f;}
    }
    // A deterministic look override is limited to visual QA; ordinary play uses
    // the editable controls above, and the grade fades at the settlement edge.
    if(const char *qa_look=std::getenv("VALWOOD_QA_VILLAGE_LOOK")) {
        float h,b,k,e,g;char extra;
        if(std::sscanf(qa_look,"%f,%f,%f,%f,%f%c",&h,&b,&k,&e,&g,&extra)==5&&
           std::isfinite(h)&&std::isfinite(b)&&std::isfinite(k)&&std::isfinite(e)&&std::isfinite(g)) {
            hour=std::clamp(h,5.5f,20.f);bearing=std::clamp(b,-180.f,180.f);
            sky_fill=std::clamp(k,.25f,2.f);exposure=std::clamp(e,.5f,1.75f);grade_strength=std::clamp(g,0.f,1.f);
        }
    }
    if(studio.loaded){const auto &l=studio.live.look;hour=l.hour;cloud=l.cloud;rain=l.rain;wind=l.wind;mist=l.fog;bearing=l.bearing;sun_strength=l.sun;sky_fill=l.sky;exposure=l.exposure;}
    if(city_showcase)grade_strength=.7f;
    else if(source_showcase)grade_strength=0.f;
    else{auto &l=studio.live.look;l.hour=hour;l.cloud=cloud;l.rain=rain;l.wind=wind;l.fog=mist;l.bearing=bearing;l.sun=sun_strength;l.sky=sky_fill;l.exposure=exposure;if(!studio.dirty)studio.draft.look=l;studio.bind();}
    float final_visual_time=17.f;
    float previous_rendered_visual_time=0.f;
    // Freeze only for deterministic visual diagnostics: keep Halton jitter and
    // rendering active while stopping wind, cloud and resident animation time.
    const bool qa_fixed_time=std::getenv("VALWOOD_QA_FIXED_TIME")!=nullptr;
    const bool qa_graphics_cycle=std::getenv("VALWOOD_QA_GRAPHICS_CYCLE")!=nullptr;
    const char *foliage_cycle_dir=launch->frames&&std::filesystem::exists("PRIVATE_CAPTURE_PROJECT.json")?std::getenv("VALWOOD_QA_FOLIAGE_CYCLE"):nullptr;
    unsigned foliage_cycle_phase=0,foliage_cycle_settled=0;bool foliage_cycle_advance=false;
    const bool qa_display_cycle=launch->frames&&std::getenv("VALWOOD_QA_DISPLAY_CYCLE")!=nullptr;
    uint64_t graphics_apply_count=0;
    const bool qa_wildlife_return=std::getenv("VALWOOD_QA_WILDLIFE_RETURN")!=nullptr;
    const bool qa_bandit_return=std::getenv("VALWOOD_QA_BANDIT_RETURN")!=nullptr;
    // Explicit capture-only actor inspection. This never affects ordinary play
    // or timed profiles; the actual scene/render path remains unchanged.
    unsigned qa_npc_follow=unsigned(world_village_resident_count);
    unsigned qa_pet_follow=unsigned(world_village_pet_count);
    if(launch->frames&&!launch->profile[0]) {
        if(const char *text=std::getenv("VALWOOD_QA_NPC_FOLLOW")) {
            char *end=nullptr;const long index=std::strtol(text,&end,10);
            if(end!=text&&!*end&&index>=0&&index<long(world_village_resident_count))qa_npc_follow=unsigned(index);
        }
        if(const char *text=std::getenv("VALWOOD_QA_PET_FOLLOW")) {
            char *end=nullptr;const long index=std::strtol(text,&end,10);
            if(end!=text&&!*end&&index>=0&&index<long(world_village_pet_count))qa_pet_follow=unsigned(index);
        }
    }
    float last_rain=0,last_fog=0,last_wind_x=0,last_wind_z=0;
    bool creek_water_prewarmed=false;
    double creek_water_prewarm_ms=0;
    unsigned creek_water_startup_passes=0;
    int water_scene_width=0,water_scene_height=0;
    uint64_t first_reflection_frame=0,reflection_play_allocations=0,sky_reflection_play_allocations=0,water_scene_play_allocations=0;
    WorldWaterMirrorCache water_mirror_cache;
    std::uint64_t water_mirror_captured_frames=0,water_mirror_reused_frames=0;
    float water_post_blend=0,water_post_depth=0,water_post_time=0;bool water_post_hands=false;
    auto compose_post=[&](GLuint scene,GLuint depth,GLuint glow) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDisable(GL_SCISSOR_TEST);
        if(studio.open){glViewport(0,0,display_w,display_h);glClearColor(.018f,.022f,.029f,1);glClear(GL_COLOR_BUFFER_BIT);}
        glViewport(designer_drawable.x,designer_drawable.y,designer_drawable.w,designer_drawable.h);
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glDisable(GL_BLEND);
        post.use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, scene);
        post.i("uScene", 0);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D,depth);
        post.i("uDepth",1);
        post.f("uNear",view_near_plane());
        post.f("uFar",15000.f);
        post.f("uExposure", exposure*auto_exposure_factor);
        post.f("uDetailSharpness",studio.live.look.sharpness);
        post.f("uPortraitFocus",portrait_focus_distance);
        const float camp_grade=studio.open?0.f:float(std::clamp(1.-(std::hypot(cam.x+11988.,cam.z-5172.)-75.)/160.,0.,1.));
        post.f("uCampGrade",camp_grade*.42f);post.f("uFilmTime",water_post_time);
        const float film_amount=graphics.environment_effects==WorldEnvironmentEffects::Low?0.f:graphics.environment_effects==WorldEnvironmentEffects::Medium?.22f:graphics.environment_effects==WorldEnvironmentEffects::High?.35f:.45f;
        post.f("uFilmGrain",studio.open?0.f:film_amount);
        post.i("uTemporalActive",temporal?1:0);
        glActiveTexture(GL_TEXTURE2);glBindTexture(GL_TEXTURE_3D,village_grade.texture);
        post.i("uVillageGrade",2);
        post.f("uVillageGradeWeight",village_grade_ready?grade_strength*(city_showcase?1.f:float(world_atlas_village_weight(cam.x,cam.z))):0.f);
        post.v2("uResolution", float(post_scene_w?post_scene_w:target.w), float(post_scene_h?post_scene_h:target.h));
        post.v2("uOutputResolution",float(designer_drawable.w),float(designer_drawable.h));
        post.f("uTaauSharpen",post_scene_w?.32f:0.f);
        glActiveTexture(GL_TEXTURE3);glBindTexture(GL_TEXTURE_2D,glow);post.i("uBloom",3);
        post.f("uBloomStrength",glow?bloom_strength:0.f);
        post.f("uUnderwaterBlend",water_post_blend);post.f("uSubmergeDepth",water_post_depth);post.f("uWaterTime",water_post_time);
        glActiveTexture(GL_TEXTURE4);glBindTexture(GL_TEXTURE_2D,water_post_hands?training_weapon.depth:0);post.i("uHandsDepth",4);post.i("uHandsActive",water_post_hands?1:0);
        glBindVertexArray(emptyVAO);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glViewport(0,0,display_w,display_h);
    };
    #include "world_demo_probe_capture.inc"
    // Candidate-only opt-in. A failed import/upload preserves the established
    // avatar and every gameplay/first-person/other-weapon path.
    WorldSkeletalHeroLoadouts skeletal_hero;skeletal_hero.bind_weapon(&fight.weapon);
    Program skeletal_first_person;
    std::string skeletal_hero_error;
    const char *skeletal_hero_path=std::getenv("VALWOOD_MODULAR_KNIGHT");
    bool skeletal_hero_loaded=skeletal_hero_path&&*skeletal_hero_path&&skeletal_hero.load_cpu(skeletal_hero_path,skeletal_hero_error);
    const char *story_civilian_option=resident_story_appearances.selected?resident_story_appearances.civilian.c_str():std::getenv("VALWOOD_STORY_CIVILIAN_APPEARANCE");
    const char *story_armored_option=resident_story_appearances.selected?resident_story_appearances.armored.c_str():std::getenv("VALWOOD_STORY_ARMORED_APPEARANCE");
    const bool story_civilian_requested=!arena_enabled&&!joust_enabled&&story_civilian_option&&*story_civilian_option;
    bool story_civilian_ready=false,story_appearance_armored=false;
    std::string story_appearance_loaded,story_appearance_pending,story_appearance_rejected;
    std::uint64_t story_appearance_request_frame=0,story_appearance_changes=0,story_appearance_failures=0,story_appearance_loading_frames=0;
    std::uint64_t story_facial_frames=0,story_blink_frames=0;
    unsigned story_facial_targets=0;float story_blink_peak=0;
    std::uint64_t story_plain_color_frames=0,story_plain_indoor_frames=0,story_plain_preloan_frames=0;
    std::uint64_t story_swim_color_frames=0,story_swim_first_person_frames=0,story_dead_color_frames=0;
    if(story_civilian_requested){
        // Reuse the accepted capture/weapon controllers, with an audited skin
        // in their exact bind space. A bad requested civilian fails startup;
        // silently showing the armored fallback would break the opening.
        story_civilian_ready=skeletal_hero_loaded&&skeletal_hero.select_appearance(story_civilian_option,skeletal_hero_error);
        if(story_civilian_ready&&(!skeletal_hero.supports_swimming()||!skeletal_hero.supports_death()||(compact_driver_enabled&&!skeletal_hero.supports_equipment()))){
            skeletal_hero_error="The story hero requires the owned swimming and defeat capture bank";story_civilian_ready=false;
        }
        if(!story_civilian_ready){std::fprintf(stderr,"[Story hero] Civilian appearance failed: %s\n",skeletal_hero_error.c_str());valid=false;}
        else{story_appearance_loaded=story_civilian_option;std::printf("[Story hero] Plain clothes loaded: %s\n",story_civilian_option);}
    }
    if(skeletal_hero_loaded){
        const char *vertex=std::getenv("VALWOOD_MODULAR_KNIGHT_SHADER");
        const char *fragment=std::getenv("VALWOOD_MODULAR_KNIGHT_FIRST_PERSON_SHADER");
        if(!vertex||!*vertex||!fragment||!*fragment||!skeletal_first_person.load(vertex,fragment)){
            skeletal_hero_error="Skeletal first-person shader failed";skeletal_hero_loaded=false;
        }
    }
    if(story_civilian_requested&&!skeletal_hero_loaded)valid=false;
    std::uint64_t skeletal_first_person_draws=0,skeletal_first_person_failures=0;
    bool skeletal_hero_active=false,skeletal_hero_draw=false,skeletal_hero_previous_support=false,skeletal_hero_root_ready=false;
    std::array<double,3> skeletal_hero_previous_root={};
    std::uint64_t skeletal_hero_hit_serial=0;
    world_character_terminal::Continuation skeletal_terminal;
    world_knight_roster::Roster arena_player_roster;
    bool arena_player_roster_ready=false;
    int arena_player_character_draft=-1;
    std::string arena_player_character_loaded,arena_player_roster_error;
    std::vector<GLuint> arena_player_portraits;
    std::vector<bool> arena_player_portrait_attempted;
    if(arena_enabled){
        const std::filesystem::path catalog="knights_roster/KNIGHT_ROSTER.cfg";
        std::error_code error;const auto bytes=std::filesystem::file_size(catalog,error);
        std::ifstream input(catalog);
        arena_player_roster_ready=!error&&bytes<=65536&&input&&world_knight_roster::read(input,arena_player_roster,arena_player_roster_error);
        if(arena_player_roster_ready){
            bool complete=arena_player_roster.entries.size()==18;
            for(unsigned n=1;n<=18;++n){char id[20];std::snprintf(id,sizeof id,"knight%02u",n);complete=complete&&arena_player_roster.index(id)>=0;}
            if(!complete){arena_player_roster_ready=false;arena_player_roster_error="The eighteen-knight roster is incomplete.";}
        }
        if(arena_player_roster_ready){
            std::string chosen=arena_player_roster.default_id;
            const auto saved=world_knight_roster::load_selection("arena_player_character.cfg",arena_player_roster,chosen,arena_player_roster_error);
            arena_player_character_draft=arena_player_roster.index(chosen);
            arena_player_portraits.resize(arena_player_roster.entries.size());arena_player_portrait_attempted.resize(arena_player_roster.entries.size());
            if(saved==world_knight_roster::SelectionLoad::Invalid||saved==world_knight_roster::SelectionLoad::IoError)
                std::fprintf(stderr,"[Arena player] Existing cosmetic choice preserved: %s\n",arena_player_roster_error.c_str());
        }else{
            if(arena_player_roster_error.empty())arena_player_roster_error="Knight appearances are unavailable in this arena.";
            std::fprintf(stderr,"[Arena player] Roster unavailable: %s\n",arena_player_roster_error.c_str());
        }
    }
    std::function<unsigned(const std::string&)> arena_player_portrait_texture=[&](const std::string &id)->unsigned{
        const int index=arena_player_roster.index(id);
        if(!arena_player_roster_ready||index<0||unsigned(index)>=arena_player_portraits.size())return 0;
        const unsigned slot=unsigned(index);
        if(!arena_player_portrait_attempted[slot]){
            arena_player_portrait_attempted[slot]=true;
            const std::string path="knights_roster/"+arena_player_roster.entries[slot].portrait_path;
            // ImGui samples display-encoded pixels; do not apply the world's
            // sRGB decoding a second time to a rendered portrait.
            arena_player_portraits[slot]=texture(path.c_str(),false);
        }
        return arena_player_portraits[slot];
    };
    std::function<bool(const std::string&,std::string&)> arena_apply_player_character=[&](const std::string &id,std::string &error){
        const int index=arena_player_roster.index(id);
        if(!arena_enabled||!arena_player_roster_ready||index<0){error="Choose an available knight before starting.";return false;}
        if(skeletal_hero_loaded&&arena_player_character_loaded==id){error.clear();return true;}
        const std::string path="knights_roster/"+arena_player_roster.entries[unsigned(index)].appearance_config;
        // select_appearance owns atomic all-four-rig CPU/GPU/FP replacement;
        // failure retains the previously committed character and its identity.
        if(!skeletal_hero.select_appearance(path.c_str(),error))return false;
        skeletal_hero_loaded=true;arena_player_character_loaded=id;arena_player_roster_error.clear();
        skeletal_hero_active=skeletal_hero_draw=skeletal_hero_previous_support=skeletal_hero_root_ready=false;
        skeletal_hero_previous_root={};skeletal_hero_hit_serial=0;skeletal_terminal.reset();avatar_initialized=false;
        std::printf("[Arena player] loaded=%s\n",id.c_str());error.clear();return true;
    };
    if(skeletal_hero_path&&*skeletal_hero_path)std::fprintf(stderr,"[Skeletal hero candidate] %s%s\n",skeletal_hero_loaded?"CPU asset loaded: ":"Fallback: ",skeletal_hero_loaded?skeletal_hero_path:skeletal_hero_error.c_str());
    std::function<bool(Program&,const Camera&,bool)> battlefield_draw_actors_callback;
    #include "world_runtime_host.inc"
    bool authored_cutscene_locked=false;
    std::function<bool(std::string&)> cutscene_progress_save,cutscene_progress_load;
    std::function<void()> cutscene_progress_reset;
    #include "world_demo_journey.inc"
    std::uint64_t npc_nameplate_frames=0,npc_nameplate_labels=0;
    // Plain-clothes exploration starts relaxed, while story progression and
    // arena setup retain their existing weapon policy. This is an initial
    // choice only: ordinary draw/stow and weapon changes remain available.
    bool weapon_sheathed=story_civilian_ready&&!first_journey_enabled;unsigned weapon_stows=0,weapon_draws=0;
    world_equipment_presentation::ActionGate equipment_action_gate;
    world_equipment_presentation::Change equipment_change;
    world_equipment_presentation::RequestFeedback equipment_request_feedback;
    std::function<bool(world_weapon_loadout::Weapon)> equipment_request_weapon;
    std::function<void(float,bool)> equipment_tick;
    std::function<bool()> equipment_actions_ready,equipment_request_draw;
    auto equipment_cancel_pending=[&](){equipment_change.cancel();equipment_action_gate.reset();};

    bool opening_controls_enabled=false,opening_movement_armed=false;
    world_battlefield_prologue::Config battlefield_config;
    world_battlefield_prologue::State battlefield_state,battlefield_pending_state;
    world_battlefield_prologue::Frame battlefield_frame,battlefield_pending_frame;
    WorldBattlefieldActors battlefield_actors;
    world_battlefield_scene::Director battlefield_director;
    world_battlefield_scene::Frame battlefield_scene_frame;
    double battlefield_scene_seconds=0;
    unsigned battlefield_quality=1;
    bool battlefield_visible=false,battlefield_pending_restore=false;
    bool battlefield_duck_pressed=false,battlefield_struggle_pressed=false,battlefield_retry_pressed=false;
    auto battlefield_release_edges=[&](){battlefield_duck_pressed=battlefield_struggle_pressed=battlefield_retry_pressed=false;};
    std::function<void(world_opening_prologue::Save&)> battlefield_attach_save;
    std::function<bool(const world_opening_prologue::Save&,std::string&)> battlefield_restore_save;
    std::function<void(world_opening_prologue::Pose&)> battlefield_checkpoint_pose;
    std::function<void()> battlefield_commit_restore,battlefield_reset,battlefield_start;
    std::function<void(float,bool)> battlefield_tick;
    std::function<bool()> battlefield_active,battlefield_cinematic,battlefield_sword_owned;
    std::function<void(world_opening_prologue::Frame&)> battlefield_apply_frame;
    std::function<bool()> battlefield_draw_hud;
    std::function<void()> battlefield_pause_options;
    world_first_journey::State first_journey_state;
    auto story_sword_unlocked=[&](){return first_journey_state.weapon_unlocked()||battlefield_state.sword_owned();};
    world_first_journey::Frame first_journey_frame;
    bool first_journey_retry_down=false,first_journey_dialogue_active=false,first_journey_defeated=false;
    unsigned first_journey_saves=0,first_journey_restores=0;
    std::function<void(world_opening_prologue::Save&)> first_journey_attach_save;
    std::function<bool(const world_opening_prologue::Save&,std::string&)> first_journey_restore_save;
    std::function<void()> first_journey_commit_restore;
    std::function<void(world_opening_prologue::Pose&)> first_journey_checkpoint_pose;
    std::function<void()> first_journey_reset;
    std::function<void(const world_opening_prologue::Input&)> first_journey_tick;
    std::function<bool(bool)> first_journey_draw;
    std::function<bool(SDL_Keycode)> opening_key;
    std::function<void(float)> opening_tick;
    std::function<void(const Camera&,bool)> opening_draw;
    std::function<void()> playtest_restart;
    std::function<bool()> playtest_use;
    std::function<bool(SDL_Keycode)> playtest_key;
    std::function<void(float)> playtest_tick;
    std::function<void(const Camera&,bool)> playtest_draw;
    world_player_inventory::State player_inventory,inventory_pending;
    bool inventory_open=false,inventory_pending_restore=false;
    std::function<void(world_opening_prologue::Save&)> inventory_attach_save;
    std::function<bool(const world_opening_prologue::Save&,std::string&)> inventory_restore_save;
    std::function<void()> inventory_commit_restore;
    std::function<bool(world_weapon_loadout::Weapon)> inventory_select_weapon;
    #include "world_presentation_runtime.inc"
    const bool world_controller_enabled=presentation||city_showcase;
    auto active_controller_context=[&](){
        if(authored_cutscene_locked)return world_controller::Context::Dialogue;
        if(presentation)return preview_controller_context();
        using C=world_controller::Context;
        if(studio.open||show_map||show_graphics||destination_loading)return C::Menu;
        if(studio.components_runtime&&studio.components_runtime->current_line())return C::Dialogue;
        return fight.engaged()?C::Combat:C::Explore;
    };
    world_controller::SwordStick sword_stick;
    if(presentation&&(stronghold_finish||city_showcase)){preview_screen=1;preview_pending_save=false;preview_chapter_time=0;if(!city_showcase)cam.fly=source_showcase&&source_viewpoint;preview_release_mouse();}
    if(skeletal_hero_loaded&&std::getenv("VALWOOD_MODULAR_KNIGHT_THIRD_PERSON"))third_person=true;
    // A player-facing showcase launch bypasses the title and keeps the
    // existing journey untouched. QA controls remain separately frame-gated.
    std::function<void()> inventory_menu_open;
    std::function<bool()> arena_begin;
    std::function<bool(SDL_Keycode)> arena_key;
    std::function<void(const Camera&,bool)> arena_draw;
    #include "world_playtest_runtime.inc"
    #include "world_player_profile_runtime.inc"
    const bool equipment_qa=arena_enabled&&launch->frames&&!launch->benchmark&&std::getenv("VALWOOD_QA_EQUIPMENT");
    if(equipment_qa){
        std::ifstream marker(".valwood-equipment-qa");std::string token;
        if(!(marker>>token)||token!="VALWOOD_PRIVATE_EQUIPMENT_V1")return 1;
        marker>>std::ws;if(!marker.eof())return 1;
    }
    const bool blade_clash_qa=arena_enabled&&launch->frames&&!launch->benchmark&&std::getenv("VALWOOD_QA_BLADE_CLASH");
    const bool animation_polish_qa=blade_clash_qa&&std::getenv("VALWOOD_QA_ANIMATION_POLISH");
    const bool arena_roster_qa=arena_enabled&&launch->frames&&!launch->benchmark&&std::getenv("VALWOOD_QA_ARENA_ROSTER");
    #include "world_combat_arena_runtime.inc"
    #include "world_arena_qa.inc"
    #include "world_orc_roster_qa.inc"
    #include "world_sword_stick_qa.inc"
    #include "world_opening_prologue_runtime.inc"
    // AAA candidate: free exploration keeps the three arrival travelers at
    // the forest camp (the story opening configures them only in demo mode).
    if(!arrival_camp_enabled&&arrival_camp&&std::getenv("VALWOOD_WORLD_EXPLORATION")&&!std::getenv("VALWOOD_QA_NO_CAMP_TRAVELERS")){
        std::ifstream camp_file("opening/arrival-camp.settings");world_opening_arrival_camp::Config camp_config;std::string camp_error;
        if(camp_file&&world_opening_arrival_camp::parse(camp_file,camp_config,camp_error)){
            WorldTrailCampPlacement camp_people[3];
            for(unsigned i=0;i<3;++i){const auto &a=camp_config.actors[i];camp_people[i]={a.feet.x,a.feet.y,a.feet.z,float(a.yaw),a.seated};}
            if(world_trail_camp_configure_arrival(arrival_camp,camp_config.center.x,camp_config.center.y,camp_config.center.z,camp_people,3)){
                arrival_camp_enabled=true;std::printf("[World] Camp travelers present for exploration: Rowan, Bram, Ellis\n");
            }else std::fprintf(stderr,"[World] Camp travelers could not be placed\n");
        }else if(camp_file)std::fprintf(stderr,"[World] Camp travelers settings invalid: %s\n",camp_error.c_str());
    }
    #include "world_first_journey_runtime.inc"
    #include "world_road_traffic_runtime.inc"
    auto story_unarmed_body=[&](){return story_civilian_ready&&first_journey_enabled&&
        !(battlefield_state.sword_owned()||(first_journey_outdoors()&&story_sword_unlocked()));};
    auto story_player_dead=[&](){return story_civilian_ready&&(fight.health<=0||fight.state==TrainingState::Lost);};
    auto story_native_swim=[&](){return story_civilian_ready&&cam.swimming&&skeletal_hero.supports_swimming();};
    auto skeletal_equipment_stowed=[&](){return (weapon_sheathed||story_unarmed_body()||story_native_swim()||story_player_dead())&&skeletal_hero.supports_equipment();};
    TrainingEvents ordinary_contact_events;
    std::array<TrainingAnatomicalHit,world_combat_capacity::npcs> combat_anatomical_hits={};
    std::array<std::uint64_t,world_combat_capacity::npcs> combat_anatomical_serials={};
    unsigned long long combat_anatomical_queries=0,combat_anatomical_body=0,combat_anatomical_steel=0,combat_anatomical_misses=0;
    unsigned long long skeletal_contact_queries=0,skeletal_contact_samples=0,skeletal_contact_failures=0;
    double skeletal_contact_pose_clock=-1;
    struct BladeSweepCache {
        std::uint64_t serial=0;double clock=-1,begin=-1,end=-1,root[3]={};float yaw=0;
        unsigned weapon=~0u,direction=~0u,count=0;std::array<WorldCombatBlade,13> blades;
    } skeletal_sweep_cache;
    struct IncomingPoseCache {
        bool ready=false,raised=false;double clock=-1,interval=-1,pose_clock=-1,root[3]={};float yaw=0;
        unsigned direction=~0u,weapon=~0u,edge_count=0;
        std::vector<world_combat_anatomy::Capsule> body;
        std::array<WorldCombatBlade,2> edges;
        std::vector<std::array<double,9>> surfaces_begin,surfaces_end;
    } incoming_pose_cache;
    // One eligibility contract drives both the displayed rig and its contact
    // geometry. Evaluate current controls here, rather than last frame's draw
    // flag, so switching view/weapon or teleporting cannot use an old blade.
    auto skeletal_hero_supported_now=[&](){return !cam.fly&&(!cam.swimming||story_native_swim())&&(!(weapon_sheathed||story_unarmed_body())||skeletal_hero.supports_equipment())&&
        WorldSkeletalHeroLoadouts::supports_weapon(fight.weapon)&&(!fight.guard||skeletal_hero.supports_guards())&&
        ((fight.health>0&&fight.state!=TrainingState::Lost)||(story_player_dead()&&skeletal_hero.supports_death()))&&
        // Quick steps use the owned backward start/walk/stop captures. Keep
        // this selected CPU rig active instead of swapping to the legacy body.
        
        (!first_journey_enabled||story_civilian_ready||(first_journey_outdoors()&&first_journey_state.weapon_unlocked()));};
    // Combat runs before this frame's animation update. A changed view/support
    // or teleported root must establish its new pose baseline before either
    // displaying or querying the new rig. A zero-time transition seed establishes
    // that baseline; ordinary attack/guard clocks never trip this gate.
    auto skeletal_hero_root_reset_pending=[&](){return !skeletal_hero_root_ready||std::hypot(
        std::hypot(cam.x-skeletal_hero_previous_root[0],cam.z-skeletal_hero_previous_root[2]),
        cam.y-1.75-skeletal_hero_previous_root[1])>=1.;};
    auto skeletal_hero_context_ready_now=[&](){return skeletal_hero_supported_now()&&skeletal_hero_previous_support&&!skeletal_hero_root_reset_pending();};
    std::function<void(WorldSkeletalHero::Input&)> skeletal_guard_target;
    auto seed_skeletal_hero_transition=[&](){
        if(!skeletal_hero_loaded||!skeletal_hero_supported_now())return false;
        if(skeletal_hero.ready()&&skeletal_hero_previous_support&&!skeletal_hero_root_reset_pending())return true;
        // Establish a changed loadout/root before committing an attack. This
        // zero-time baseline neither advances gameplay nor borrows another
        // weapon's last rendered palette for the first anatomical query.
        WorldSkeletalHeroLoadouts::Input input;input.x=cam.x;input.y=cam.y-1.75;input.z=cam.z;
        input.yaw=3.14159265359f-cam.yaw;input.reset=true;input.grounded=cam.grounded;input.equipment_stowed=skeletal_equipment_stowed();
        input.swimming=story_native_swim();input.dead=story_player_dead();
        float water_surface=0;input.submerged=input.swimming&&world_atlas_local_water(cam.x,cam.z,&water_surface,nullptr)&&world_swimming::submerged(cam.y,water_surface);
        input.reaction_serial=skeletal_hero_hit_serial;input.reaction_direction=unsigned(fight.enemy_direction);
        if(skeletal_guard_target)skeletal_guard_target(input);
        if(!skeletal_hero.update_cpu(input,fight,skeletal_hero_error)||
           !skeletal_hero.prepare(live_programs.skeletal.id,live_programs.skeletal_shadow.id,world_program_epoch,skeletal_hero_error)||
           !skeletal_hero.prepare_first_person(skeletal_first_person.id,world_program_epoch,skeletal_hero_error)){
            std::fprintf(stderr,"[Skeletal hero candidate] Transition failed: %s\n",skeletal_hero_error.c_str());
            skeletal_hero_loaded=false;if(story_civilian_ready){launch->exit_code=1;running=false;}return false;
        }
        skeletal_hero_previous_root={cam.x,cam.y-1.75,cam.z};skeletal_hero_root_ready=true;
        skeletal_hero_previous_support=true;skeletal_contact_pose_clock=fight.clock;skeletal_sweep_cache.count=0;incoming_pose_cache.ready=false;return true;
    };
    auto sync_skeletal_hero_combat_timing=[&](){seed_skeletal_hero_transition();fight.sword_high_windup_min_seconds=
        skeletal_hero_loaded&&skeletal_hero.ready()&&skeletal_hero_supported_now()?.36:0.;};
    // Refresh before committing input, including the first unsheathing swing.
    // The combat clock latches this duration; changing view never retimes a
    // swing that has already started. Buffered combos refresh before ticking.
    equipment_actions_ready=[&](){return !story_unarmed_body()&&!weapon_sheathed&&!equipment_change.active()&&(equipment_action_gate.armed()||blade_clash_qa||arena_roster_qa)&&
        (!skeletal_hero_loaded||!WorldSkeletalHeroLoadouts::supports_weapon(fight.weapon)||!skeletal_hero.supports_equipment()||skeletal_hero.equipment_combat_ready());};
    equipment_request_draw=[&](){if(story_unarmed_body()||equipment_change.active()||!weapon_sheathed)return false;weapon_sheathed=false;++weapon_draws;equipment_action_gate.reset();fight.set_guard(false);fight.clear_buffer();sword_stick.reset();return true;};
    auto skeletal_timed_attack=[&](){if(!equipment_actions_ready()){fight.clear_buffer();return false;}sync_skeletal_hero_combat_timing();return fight.attack();};
    std::function<bool(const TrainingAnatomicalQuery &,TrainingAnatomicalHit &)> combat_anatomical_query=
        [&](const TrainingAnatomicalQuery &query,TrainingAnatomicalHit &hit){
            hit={};++combat_anatomical_queries;
            if(!arena_opponent_anatomy_ready(query.opponent_id)||query.opponent_id>=world_combat_capacity::opponents){++combat_anatomical_misses;return false;}
            WorldCombatContactFrame contact;
            contact.attacker_motion=combat_motion;
            contact.player[0]=cam.x;contact.player[1]=cam.y-1.75;contact.player[2]=cam.z;
            contact.player_yaw=3.14159265359f-cam.yaw;
            if(arena_enabled){
                const auto &actor=arena_game.challengers()[query.opponent_id];
                contact.defender[0]=arena_origin_x+actor.position.x;
                contact.defender[1]=arena_floor+actor.position.y;
                contact.defender[2]=arena_origin_z+actor.position.z;
            }else{
                double ox=0,oz=0;world_atlas_village_origin(&ox,&oz);
                if(presentation){ox=preview_arena_x;oz=preview_arena_z;}
                contact.defender[0]=ox+fight.enemy_x;
                contact.defender[1]=presentation?preview_arena_feet:24.;
                contact.defender[2]=oz+fight.enemy_z;
            }
            if(!world_combat_motion_facing(arena_opponent_motion(query.opponent_id),query.opponent_id+1,contact.defender_yaw))
                contact.defender_yaw=float(std::atan2(cam.x-contact.defender[0],cam.z-contact.defender[2]));
            if(query.incoming){
                // Query the same committed CPU joints and held guard used by
                // the displayed hero. A matching direction alone never blocks.
                if(!skeletal_hero_loaded||!skeletal_hero.ready()||skeletal_hero_root_reset_pending()){
                    ++combat_anatomical_misses;return false;
                }
                WorldCombatIncomingFrame incoming;
                for(unsigned n=0;n<3;++n)incoming.opponent[n]=contact.defender[n];
                incoming.opponent_yaw=contact.defender_yaw;
                const double elapsed=std::clamp(query.contact_clock-skeletal_contact_pose_clock,0.,.10);
                const bool guard_pose=query.weapon_guard&&skeletal_hero_context_ready_now();
                auto &cache=incoming_pose_cache;
                if(!cache.ready||cache.clock!=query.contact_clock||cache.interval!=query.interval_seconds||cache.pose_clock!=skeletal_contact_pose_clock||
                   cache.raised!=guard_pose||cache.direction!=unsigned(query.defender_guard)||cache.weapon!=unsigned(query.weapon)||cache.yaw!=contact.player_yaw||
                   cache.root[0]!=contact.player[0]||cache.root[1]!=contact.player[1]||cache.root[2]!=contact.player[2]){
                    cache.ready=false;cache.edge_count=0;cache.surfaces_begin.clear();cache.surfaces_end.clear();
                    if(!skeletal_hero.body_capsules(contact.player[0],contact.player[1],contact.player[2],contact.player_yaw,cache.body,skeletal_hero_error,guard_pose?unsigned(query.defender_guard):~0u,guard_pose?elapsed:0.)){
                        ++skeletal_contact_failures;++combat_anatomical_misses;return false;
                    }
                    if(guard_pose){
                        const double before=std::max(0.,elapsed-query.interval_seconds);bool guard_ready=true;
                        for(unsigned n=0;n<2;++n){auto &edge=cache.edges[n];
                            if(!skeletal_hero.guard_blade(unsigned(query.defender_guard),n?elapsed:before,
                                contact.player[0],contact.player[1],contact.player[2],contact.player_yaw,
                                edge.base,edge.tip,skeletal_hero_error,query.weapon)){guard_ready=false;break;}
                        }
                        if(guard_ready)cache.edge_count=2;
                        if(!skeletal_hero.guard_shield(unsigned(query.defender_guard),before,contact.player[0],contact.player[1],contact.player[2],contact.player_yaw,cache.surfaces_begin,skeletal_hero_error,query.weapon)||
                           !skeletal_hero.guard_shield(unsigned(query.defender_guard),elapsed,contact.player[0],contact.player[1],contact.player[2],contact.player_yaw,cache.surfaces_end,skeletal_hero_error,query.weapon)||
                           cache.surfaces_begin.size()!=cache.surfaces_end.size()){
                            ++skeletal_contact_failures;cache.surfaces_begin.clear();cache.surfaces_end.clear();
                        }
                    }
                    cache.clock=query.contact_clock;cache.interval=query.interval_seconds;cache.pose_clock=skeletal_contact_pose_clock;
                    cache.raised=guard_pose;cache.direction=unsigned(query.defender_guard);cache.weapon=unsigned(query.weapon);cache.yaw=contact.player_yaw;
                    std::copy_n(contact.player,3,cache.root);cache.ready=true;
                }
                incoming.defender_body=cache.body.data();incoming.defender_body_count=unsigned(cache.body.size());
                if(cache.edge_count){incoming.defender_blades=cache.edges.data();incoming.defender_blade_count=cache.edge_count;}
                if(!cache.surfaces_begin.empty()){
                    incoming.defender_guard_triangles=cache.surfaces_begin.data();incoming.defender_guard_triangles_end=cache.surfaces_end.data();
                    incoming.defender_guard_triangle_count=unsigned(cache.surfaces_begin.size());
                }
                if(!world_combat_motion_incoming_contact(arena_opponent_motion(query.opponent_id),query.opponent_id,query,incoming,hit)||!hit.valid){
                    ++combat_anatomical_misses;return false;
                }
                if(hit.weapon_contact)++combat_anatomical_steel;else ++combat_anatomical_body;
                return true;
            }
            if(skeletal_hero_loaded&&skeletal_hero.ready()&&skeletal_hero_context_ready_now()){
                ++skeletal_contact_queries;
                auto &cache=skeletal_sweep_cache;
                if(!cache.count||cache.serial!=query.attack_serial||cache.clock!=query.contact_clock||cache.begin!=query.phase_begin||cache.end!=query.phase_end||
                   cache.weapon!=unsigned(query.weapon)||cache.direction!=unsigned(query.direction)||cache.yaw!=contact.player_yaw||
                   cache.root[0]!=contact.player[0]||cache.root[1]!=contact.player[1]||cache.root[2]!=contact.player[2]){
                    cache.count=0;
                    const unsigned segments=unsigned(std::clamp(std::ceil(std::max(0.,query.phase_end-query.phase_begin)/.012),1.,12.));
                    for(unsigned i=0;i<=segments;++i){auto &blade=cache.blades[i];
                        const double phase=query.phase_begin+(query.phase_end-query.phase_begin)*double(i)/segments;
                        if(!skeletal_hero.contact_blade(unsigned(query.direction),phase,contact.player[0],contact.player[1],contact.player[2],contact.player_yaw,blade.base,blade.tip,skeletal_hero_error,query.windup_seconds,query.attack_serial,query.strike_seconds,query.recovery_seconds,query.attack_commit_clock,query.weapon)){
                            ++skeletal_contact_failures;++combat_anatomical_misses;
                            std::fprintf(stderr,"[Skeletal hero candidate] Contact rejected: %s\n",skeletal_hero_error.c_str());return false;
                        }
                        ++skeletal_contact_samples;
                    }
                    cache.serial=query.attack_serial;cache.clock=query.contact_clock;cache.begin=query.phase_begin;cache.end=query.phase_end;
                    cache.weapon=unsigned(query.weapon);cache.direction=unsigned(query.direction);cache.yaw=contact.player_yaw;
                    for(unsigned n=0;n<3;++n)cache.root[n]=contact.player[n];cache.count=segments+1;
                }
                contact.attacker_blades=cache.blades.data();contact.attacker_blade_count=cache.count;
            }else world_combat_motion_facing(combat_motion,0,contact.player_yaw);
            if(!world_combat_motion_contact(arena_opponent_motion(query.opponent_id),query,contact,hit)||!hit.valid){++combat_anatomical_misses;return false;}
            combat_anatomical_hits[query.opponent_id]=hit;
            combat_anatomical_serials[query.opponent_id]=query.attack_serial;
            skeletal_terminal.record(query,hit,contact.player_yaw,
                skeletal_hero_loaded&&skeletal_hero.ready()&&skeletal_hero_context_ready_now());
            if(hit.weapon_contact)++combat_anatomical_steel;else ++combat_anatomical_body;
            return true;
        };
    // Both NPC teams use the same posed source blade/body surfaces as their
    // rendered actors. Translate the arena-local contract exactly once.
    std::function<bool(const world_arena_combat::TeamContactQuery&,TrainingAnatomicalHit&)> arena_team_query=
        [&](const world_arena_combat::TeamContactQuery &local,TrainingAnatomicalHit &hit){
            if(!arena_enabled||local.attacker_id>=world_combat_capacity::npcs||local.defender_id>=world_combat_capacity::npcs||
               arena_game.teammate(local.attacker_id)==arena_game.teammate(local.defender_id)||
               !arena_actor_banks[local.attacker_id]||!arena_actor_banks[local.defender_id])return false;
            auto query=local;
            for(auto *root:{&query.attacker_position,&query.defender_position}){
                (*root)[0]+=arena_origin_x;(*root)[1]+=arena_floor;(*root)[2]+=arena_origin_z;
            }
            float yaw=0;
            if(world_combat_motion_facing(arena_opponent_motion(query.attacker_id),query.attacker_id+1,yaw))query.attacker_yaw=yaw;
            if(world_combat_motion_facing(arena_opponent_motion(query.defender_id),query.defender_id+1,yaw))query.defender_yaw=yaw;
            if(!world_combat_motion_team_contact(arena_opponent_motion(query.attacker_id),arena_opponent_motion(query.defender_id),query,hit)||!hit.valid)return false;
            hit.point[0]-=arena_origin_x;hit.point[1]-=arena_floor;hit.point[2]-=arena_origin_z;
            return true;
        };
    arena_game.set_team_contact_query([](const world_arena_combat::TeamContactQuery &query,TrainingAnatomicalHit &hit,void *context){
        return (*static_cast<decltype(arena_team_query)*>(context))(query,hit);
    },&arena_team_query);
    // A held matching guard may adjust within the rig's actual arm reach to
    // meet an approaching cut. This supplies a visible IK target only: damage
    // and blocks still require the ordinary blade/shield geometry query.
    struct GuardTargetCache {bool ready=false,adapt=false;std::uint64_t serial=0;unsigned direction=0,weapon=0,run=0;double clock=-1,steel_phase=.60,body_phase=.60;
        double player[3]={},opponent[3]={};float yaw=0,opponent_yaw=0;std::array<double,3> target={};};
    std::array<GuardTargetCache,world_combat_capacity::npcs> guard_target_cache;
    skeletal_guard_target=[&](WorldSkeletalHero::Input &input){
        input.guard_target_valid=false;
        if(!fight.guard||fight.player_phase!=TrainingPlayerPhase::Ready||!skeletal_hero.ready()||weapon_sheathed||fight.weapon==world_weapon_loadout::Weapon::SwordShield)return;
        std::vector<world_combat_anatomy::Capsule> posed,body;
        if(!skeletal_hero.body_capsules(input.x,input.y,input.z,input.yaw,posed,skeletal_hero_error))return;
        body=std::move(posed);
        if(body.empty())return;
        double soonest=1e9;
        for(unsigned id=0;id<(arena_enabled?unsigned(arena_game.challengers().size()):1u);++id){
            if(arena_enabled&&(arena_game.teammate(id)||arena_game.target(id)!=world_arena_combat::Arena::player_target))continue;
            if(!arena_opponent_anatomy_ready(id))continue;
            const auto view=arena_enabled?arena_game.view(fight,id):fight;
            if(view.enemy_health<=0||view.enemy_resolved||view.enemy_direction!=fight.selected||
               (view.enemy_phase!=TrainingEnemyPhase::Windup&&view.enemy_phase!=TrainingEnemyPhase::Strike))continue;
            WorldCombatIncomingFrame frame;frame.defender_body=body.data();frame.defender_body_count=unsigned(body.size());
            if(arena_enabled){const auto &actor=arena_game.challengers()[id];frame.opponent[0]=arena_origin_x+actor.position.x;frame.opponent[1]=arena_floor+actor.position.y;frame.opponent[2]=arena_origin_z+actor.position.z;}
            else{double ox=0,oz=0;world_atlas_village_origin(&ox,&oz);if(presentation){ox=preview_arena_x;oz=preview_arena_z;}
                frame.opponent[0]=ox+fight.enemy_x;frame.opponent[1]=presentation?preview_arena_feet:24.;frame.opponent[2]=oz+fight.enemy_z;}
            const double dx=frame.opponent[0]-input.x,dz=frame.opponent[2]-input.z,distance=std::hypot(dx,dz);
            if(distance<.4||distance>3.||dx*std::sin(input.yaw)+dz*std::cos(input.yaw)<distance*.1)continue;
            if(!world_combat_motion_facing(arena_opponent_motion(id),id+1,frame.opponent_yaw))frame.opponent_yaw=float(std::atan2(-dx,-dz));
            TrainingAnatomicalQuery query;query.incoming=true;query.opponent_id=id;query.weapon=fight.weapon;query.direction=view.enemy_direction;
            query.attack_serial=view.enemy_attack_start_serial;query.severed_parts=view.enemy_severed_parts;
            query.phase_begin=view.enemy_phase==TrainingEnemyPhase::Windup?
                std::max(.26,.36*std::clamp(view.enemy_time/std::max(.001,view.enemy_duration),0.,1.)):
                .36+.24*std::clamp(view.enemy_time/std::max(.001,view.enemy_duration),0.,1.);
            query.phase_end=.60;if(query.phase_begin>=query.phase_end)continue;
            query.windup_seconds=view.enemy_attack_windup_seconds;query.strike_seconds=view.enemy_phase==TrainingEnemyPhase::Strike?view.enemy_duration:world_combat_timing::incoming_strike(unsigned(view.enemy_direction),view.enemy_attack_windup_seconds,view.round);
            query.contact_clock=fight.clock;query.interval_seconds=.1;
            auto elapsed=[&](double phase){return phase<=.36?phase/.36*query.windup_seconds:query.windup_seconds+(phase-.36)/.24*query.strike_seconds;};
            const double age=view.enemy_phase==TrainingEnemyPhase::Windup?view.enemy_time:query.windup_seconds+view.enemy_time;
            auto &cache=guard_target_cache[id];
            const auto changed=[](const double *a,const double *b){return std::hypot(std::hypot(a[0]-b[0],a[2]-b[2]),a[1]-b[1])>.08;};
            const double player_root[]={input.x,input.y,input.z};
            const bool stale=!cache.ready||query.phase_begin>cache.steel_phase+.001||cache.run!=arena_appearance_runs||fight.clock+1e-6<cache.clock||cache.serial!=query.attack_serial||cache.direction!=unsigned(view.enemy_direction)||cache.weapon!=unsigned(fight.weapon)||
               changed(cache.player,player_root)||changed(cache.opponent,frame.opponent)||std::abs(cache.yaw-input.yaw)>.08f||std::abs(cache.opponent_yaw-frame.opponent_yaw)>.12f;
            if(!stale){
                const double until=std::max(0.,elapsed(cache.body_phase)-age);cache.clock=fight.clock;
                if(until<soonest){soonest=until;input.guard_target_valid=cache.adapt;input.guard_target_direction=unsigned(view.enemy_direction);input.guard_target_world=cache.target;}
                continue;
            }
            // Subdivide broad forecasts; real contacts run at 240 Hz. This
            // keeps a coarse curved sweep from choosing a grazing guard.
            auto body_forecast=[&](const WorldCombatIncomingFrame &geometry,TrainingAnatomicalHit &result,bool guarded=false){
                for(unsigned n=0;n<8;++n){auto part=query;
                    part.weapon_guard=guarded;part.defender_guard=view.enemy_direction;
                    part.phase_begin=query.phase_begin+(query.phase_end-query.phase_begin)*n/8.;
                    part.phase_end=query.phase_begin+(query.phase_end-query.phase_begin)*(n+1)/8.;
                    TrainingAnatomicalHit sample;
                    if(world_combat_motion_incoming_contact(arena_opponent_motion(id),id,part,geometry,sample)&&sample.valid){
                        result=sample;result.sweep_t=(n+sample.sweep_t)/8.;return true;
                    }
                }return false;
            };
            TrainingAnatomicalHit hit;
            if(!body_forecast(frame,hit)||hit.weapon_contact)continue;
            const double linear=query.phase_begin+(query.phase_end-query.phase_begin)*hit.sweep_t;
            const double until=std::max(0.,elapsed(linear)-age);
            if(until>=soonest)continue;
            {
                cache.ready=true;cache.adapt=false;cache.steel_phase=.60;cache.body_phase=linear;cache.run=arena_appearance_runs;cache.serial=query.attack_serial;cache.direction=unsigned(view.enemy_direction);cache.weapon=unsigned(fight.weapon);
                std::copy_n(player_root,3,cache.player);std::copy_n(frame.opponent,3,cache.opponent);cache.yaw=input.yaw;cache.opponent_yaw=frame.opponent_yaw;
                auto intercepts=[&](const std::array<double,3> &target,bool adapt,double *clearance=nullptr){
                    // A stationary preview needs only two identical endpoints.
                    // Eight forecast spans already subdivide the NPC curve.
                    std::array<WorldCombatBlade,2> edges;std::vector<world_combat_anatomy::Capsule> fitted_body;
                    std::vector<std::array<double,9>> guard_surfaces;
                    if(!skeletal_hero.guard_target_preview(unsigned(view.enemy_direction),adapt?target.data():nullptr,input.x,input.y,input.z,input.yaw,
                        edges[0].base,edges[0].tip,fitted_body,skeletal_hero_error,fight.weapon,&guard_surfaces))return false;
                    std::fill(edges.begin()+1,edges.end(),edges[0]);auto preview=frame;preview.defender_blades=edges.data();preview.defender_blade_count=unsigned(edges.size());
                    preview.defender_body=fitted_body.data();preview.defender_body_count=unsigned(fitted_body.size());
                    if(!guard_surfaces.empty()){preview.defender_guard_triangles=guard_surfaces.data();preview.defender_guard_triangle_count=unsigned(guard_surfaces.size());}
                    TrainingAnatomicalHit fitted_hit;
                    if(!body_forecast(preview,fitted_hit,true)||!fitted_hit.valid||!fitted_hit.weapon_contact)return false;
                    const double steel=query.phase_begin+(query.phase_end-query.phase_begin)*fitted_hit.sweep_t;
                    auto unguarded=preview;unguarded.defender_blades=nullptr;unguarded.defender_blade_count=0;
                    unguarded.defender_guard_triangles=nullptr;unguarded.defender_guard_triangles_end=nullptr;unguarded.defender_guard_triangle_count=0;
                    TrainingAnatomicalHit flesh;double margin=1.;
                    if(body_forecast(unguarded,flesh)){
                        const double body_phase=query.phase_begin+(query.phase_end-query.phase_begin)*flesh.sweep_t;
                        margin=elapsed(body_phase)-elapsed(steel);
                        // Fast guard-entry cuts can have a genuine steel-first
                        // crossing less than 5 ms before flesh. Preserve that
                        // contact order; actual moving poses still have to meet.
                        if(margin<(adapt?.000001:.015))return false;
                    }
                    if(clearance)*clearance=margin;
                    cache.steel_phase=steel;return true;
                };
                const std::array<double,3> unused={};
                if(!intercepts(unused,false)){
                    // Follow an actual point on the cutting edge backwards in
                    // time. Side cuts approach laterally; a front-only target
                    // would move an otherwise useful guard out of their path.
                    std::vector<std::array<double,3>> candidates;
                    auto append_targets=[&](const TrainingAnatomicalHit &flesh){
                        const double at=query.phase_begin+(query.phase_end-query.phase_begin)*flesh.sweep_t;
                        WorldCombatBlade flesh_edge;
                        if(world_combat_motion_incoming_blade(arena_opponent_motion(id),id,query,frame,at,flesh_edge)){
                            double length2=0,dot=0;for(unsigned n=0;n<3;++n){const double v=flesh_edge.tip[n]-flesh_edge.base[n];length2+=v*v;dot+=(flesh.point[n]-flesh_edge.base[n])*v;}
                            const double u=std::clamp(dot/std::max(1e-12,length2),0.,1.);
                            for(double earlier:{.080,.050,.030,.015}){
                                const double t=std::max(elapsed(query.phase_begin),elapsed(at)-earlier);
                                const double phase=t<=query.windup_seconds?.36*t/query.windup_seconds:.36+.24*(t-query.windup_seconds)/query.strike_seconds;
                                WorldCombatBlade edge;if(!world_combat_motion_incoming_blade(arena_opponent_motion(id),id,query,frame,phase,edge))continue;
                                std::array<double,3> point;for(unsigned n=0;n<3;++n)point[n]=edge.base[n]+u*(edge.tip[n]-edge.base[n]);candidates.push_back(point);
                            }
                        }
                        for(double offset:{.28,.40,0.})candidates.push_back({flesh.point[0]+dx/distance*offset,flesh.point[1],flesh.point[2]+dz/distance*offset});
                    };
                    append_targets(hit);
                    // Try a central-body approach as well: moving the guard
                    // changes its arm positions. Both sets of candidate poses
                    // must still stop the cut before ANY of the12 real caps.
                    std::vector<world_combat_anatomy::Capsule> central;
                    for(const auto &cap:body)if(cap.part==TrainingBodyPart::Head||cap.part==TrainingBodyPart::Torso||cap.part==TrainingBodyPart::LeftLeg||cap.part==TrainingBodyPart::RightLeg)central.push_back(cap);
                    if(!central.empty()){
                        auto centre_frame=frame;centre_frame.defender_body=central.data();centre_frame.defender_body_count=unsigned(central.size());
                        TrainingAnatomicalHit centre_hit;if(body_forecast(centre_frame,centre_hit)&&std::abs(centre_hit.sweep_t-hit.sweep_t)>.001)append_targets(centre_hit);
                    }
                    double best_margin=-1,best_steel=.60;
                    for(const auto &target:candidates){double margin=0;
                        if(intercepts(target,true,&margin)&&margin>best_margin+1e-9){
                            best_margin=margin;best_steel=cache.steel_phase;cache.target=target;cache.adapt=true;
                        }
                    }
                    if(cache.adapt)cache.steel_phase=best_steel;
                }
            }
            cache.clock=fight.clock;soonest=until;input.guard_target_valid=cache.adapt;
            input.guard_target_direction=unsigned(view.enemy_direction);input.guard_target_world=cache.target;
        }
    };
    auto story_weapon=world_weapon_loadout::Weapon::Sword;
    auto story_weapon_ready=[&](){return !(battlefield_frame.active&&battlefield_frame.cinematic)&&!arena_enabled&&world_combat_motion_loadouts_ready(combat_motion)&&first_journey_enabled&&(battlefield_state.sword_owned()||first_journey_outdoors())&&story_sword_unlocked()&&preview_screen==1&&!first_journey_dialogue_active&&!show_map&&!show_graphics&&!destination_loading&&!cam.swimming;};
    auto story_equip=[&](world_weapon_loadout::Weapon weapon){
        if(!story_weapon_ready()||weapon==world_weapon_loadout::Weapon::Bow||!world_weapon_loadout::valid(weapon))return;
        if(fight.player_phase!=TrainingPlayerPhase::Ready){playtest_tell("Finish the swing before switching weapons.");return;}
        if(equipment_request_weapon&&equipment_request_weapon(weapon))return;
        if(inventory_select_weapon&&!inventory_select_weapon(weapon))return;
        story_weapon=weapon;fight.weapon=weapon;weapon_sheathed=false;fight.set_guard(false);fight.clear_buffer();sword_stick.reset();
        playtest_tell(std::string(world_weapon_loadout::profile(weapon).name)+" equipped.");
    };
    #include "world_player_inventory_runtime.inc"
    #include "world_battlefield_runtime.inc"
    #include "world_battlefield_render_runtime.inc"
    battlefield_draw_actors_callback=[&](Program &program,const Camera &eye,bool shadow){return battlefield_draw(program,eye,shadow);};
    auto story_sync_appearance=[&](std::uint64_t current_frame){
        if(!story_civilian_ready||!skeletal_hero_loaded||!story_armored_option||!*story_armored_option)return;
        // A sword loan is not a knight transformation. Only explicitly
        // equipped roadwarden mail changes the body, after leaving home.
        const bool armored=(!first_journey_enabled||(first_journey_outdoors()&&first_journey_state.weapon_unlocked()))&&
            player_inventory.armor[unsigned(world_player_inventory::Slot::Chest)]==world_player_inventory::Item::Mail;
        const std::string desired=armored?story_armored_option:story_civilian_option;
        if(desired==story_appearance_loaded){story_appearance_rejected.clear();story_appearance_pending.clear();return;}
        if(desired==story_appearance_rejected)return;
        auto safe_boundary=[&](){return !fight.engaged()&&fight.health>0&&fight.state!=TrainingState::Lost&&fight.player_phase==TrainingPlayerPhase::Ready&&!equipment_change.active()&&
            !skeletal_hero.stats().equipment_transition&&!authored_cutscene_locked&&!cam.swimming&&
            !(opening_enabled&&(opening_frame.cinematic||opening_frame.camp_dialogue||first_journey_dialogue_active));};
        if(story_appearance_pending.empty()){
            // Never replace the physical blade/body during a committed move.
            // Present the ordinary loading image before synchronous uploads.
            if(!safe_boundary())return;
            story_appearance_pending=desired;story_appearance_request_frame=current_frame;destination_loading=true;return;
        }
        if(current_frame<=story_appearance_request_frame)return;
        if(story_appearance_pending!=desired){story_appearance_pending.clear();return;}
        // Recheck immediately before ownership changes, even if another
        // gameplay system published an action after the original request.
        if(!safe_boundary()){story_appearance_pending.clear();return;}
        std::string error;
        if(!skeletal_hero.select_appearance(desired.c_str(),error)){
            // select_appearance is atomic: old CPU/GPU/first-person resources
            // remain usable. Retry only after a different equipment request.
            story_appearance_rejected=desired;++story_appearance_failures;
            opening_tell("That outfit could not load. Your current appearance was kept.");
            std::fprintf(stderr,"[Story hero] Appearance retained: %s\n",error.c_str());
        }else{
            story_appearance_loaded=desired;story_appearance_armored=armored;++story_appearance_changes;
            skeletal_hero_active=skeletal_hero_draw=skeletal_hero_previous_support=skeletal_hero_root_ready=false;
            skeletal_hero_previous_root={};skeletal_hero_hit_serial=0;skeletal_terminal.reset();avatar_initialized=false;
            skeletal_contact_pose_clock=-1;skeletal_sweep_cache.count=0;incoming_pose_cache.ready=false;
            temporal_cut=true;std::printf("[Story hero] Equipped appearance: %s\n",armored?"roadwarden armor":"plain clothes");
        }
        story_appearance_pending.clear();
    };
    #include "world_equipment_runtime.inc"
    const auto equipment_request_weapon_supported=equipment_request_weapon;
    equipment_request_weapon=[&,equipment_request_weapon_supported](world_weapon_loadout::Weapon weapon){
        if(story_unarmed_body()){
            equipment_request_feedback.set(world_equipment_presentation::RequestResult::Rejected,"Speak to Bram before drawing or changing weapons.");
            inventory_message=equipment_request_feedback.message;return true;
        }
        return equipment_request_weapon_supported(weapon);
    };
    auto story_count_color_draw=[&](std::uint64_t before){
        if(!story_civilian_ready||!skeletal_hero_draw||skeletal_hero.stats().draws<=before)return;
        if(story_native_swim())++story_swim_color_frames;
        if(story_player_dead())++story_dead_color_frames;
        if(story_appearance_armored)return;
        ++story_plain_color_frames;
        if(opening_enabled&&opening_state.stage()==world_opening_prologue::Stage::HouseExplore)++story_plain_indoor_frames;
        if(first_journey_enabled&&first_journey_outdoors()&&!first_journey_state.weapon_unlocked())++story_plain_preloan_frames;
    };
    first_journey_loot_prompt_available=inventory_loot_near;
    // The opening host has now applied its actual interior lighting values.
    if(running&&opening_enabled&&opening_interior.enabled&&!ensure_local_probes(cam)){launch->exit_code=1;running=false;}
    #include "world_playtest_combat_qa.inc"
    #include "world_combat_feedback_qa.inc"
    #include "world_arena_settings_qa.inc"
    #include "world_arena_roster_qa.inc"
    std::function<bool(std::uint64_t)> animation_polish_clash_hold;
    #include "world_blade_clash_qa.inc"
    #include "world_animation_polish_qa.inc"
    #include "world_severing_qa.inc"
    #include "world_grandstone_obstacle_qa.inc"
    if(presentation&&!playtest_enabled&&!opening_enabled&&milestone_finish&&!launch->benchmark)if(const char *view=std::getenv("VALWOOD_SHOWCASE")){
        if(!std::strcmp(view,"jungle")||!std::strcmp(view,"court")||!std::strcmp(view,"hero")){
            preview_review(!std::strcmp(view,"jungle")?world_milestone::View::Jungle:!std::strcmp(view,"hero")?world_milestone::View::Hero:world_milestone::View::Courtyard);
            std::printf("[Preview] Direct showcase: %s; journey saves are unchanged\n",view);
        }
    }
    if(presentation&&launch->frames&&milestone_finish)if(const char *view=std::getenv("VALWOOD_QA_MILESTONE_VIEW")){
        preview_review(!std::strcmp(view,"hero")?world_milestone::View::Hero:!std::strcmp(view,"jungle")?world_milestone::View::Jungle:world_milestone::View::Courtyard);
    }
    if(presentation&&launch->frames&&jungle_expanded)if(const char *view=std::getenv("VALWOOD_QA_JUNGLE_VIEW")){
        const int index=std::atoi(view);
        if(index==5){cam.x=-26704.;cam.z=9723.496705929329;cam.y=world_atlas_height(cam.x,cam.z)+32.;cam.yaw=-.6f;cam.pitch=0;cam.fly=true;cam.grounded=false;third_person=false;destination_loading=true;temporal_cut=true;}
        if(index>=0&&index<int(std::size(kJunglePlaces))){
            const auto &place=kJunglePlaces[index];cam.x=place.x+10;cam.z=place.z+13;
            cam.y=world_atlas_height(cam.x,cam.z)+1.75;cam.yaw=std::atan2(-10.f,13.f);cam.pitch=-.04f;
            cam.fly=false;cam.grounded=true;avatar_initialized=false;
            destination_loading=true;temporal_cut=true;
        }
    }
    running=valid;
    // Startup and play exercise exactly the same reflection/material paths.
    // Startup uses no timing queries or walking-frame counters.
    auto draw_water_reflection=[&](const Camera &cam,const Lighting &light,const WorldTorchLights &torch_lights,
            const M4 &lightVP,float visual_time,float water_level,float wind_x,float wind_z,float wetness,
            float motion_delta,M4 &reflection_vp,bool record_stats,uint64_t frame,bool builtin_pool_only=true,bool sole_river_consumer=false,const M4 *river_main_vp=nullptr)->bool {
        Camera mirror=cam;mirror.y=2*water_level-cam.y;mirror.pitch=-cam.pitch;
        V3 mirror_forward=mirror.forward(),mirror_right=mirror.right(),mirror_up=norm(cross(mirror_right,mirror_forward));
        reflection_vp=product(perspective(cam.fov,cam.aspect,view_near_plane(),15000.f),look({0,0,0},mirror_forward,{0,1,0}));
        M4 shift=identity();shift.m[13]=float(2*(water_level-cam.y));
        M4 reflected_light=product(lightVP,shift);
        auto prepare_reflected_foliage_program=[&](Program &program) {
            program.use();program.i("uArchitectureWeathering",1);program.i("uShadowGather",shadow_gather_enabled?1:0);program.i("uFacadeMode",0);program.i("uClipBelowWater",1);program.f("uWaterClipLevel",water_level);
            landscape.bind_shelter(program);studio_surface_gpu.bind(program,cam.x,cam.y,cam.z);
            program.m4("uVP",reflection_vp);program.m4("uLightVP",reflected_light);light_program(program,light,mirror);torch_program(program,torch_lights);bind_local_probes(program,mirror);
            weather_program(program,wind_x,wind_z,wetness);
            program.f("uTemporalPhase",0);program.i("uWindHistoryEnabled",1);program.f("uWindFrameDelta",motion_delta);
            program.f("uTime",visual_time);
            glActiveTexture(GL_TEXTURE7);glBindTexture(GL_TEXTURE_2D,shadow.depth);program.i("uShadow",7);
            program.f("uPixelScale",float(reflection.h)/(2*std::tan(cam.fov*.5f)));
            program.v2("uWorldSize",float(world_atlas_width()),float(world_atlas_depth()));
            glActiveTexture(GL_TEXTURE11);glBindTexture(GL_TEXTURE_2D,macro_height);program.i("uMacroHeight",11);
        };
        auto prepare_reflected_foliage=[&](){prepare_reflected_foliage_program(foliage);};
        glBindFramebuffer(GL_FRAMEBUFFER,reflection.fbo);
        glViewport(0,0,reflection.w,reflection.h);
        if(!record_stats&&wildlife_preparation_enabled&&world_wildlife_stats(wildlife).pose_assets) {
            // The loading caller owns this single-color reflection target.
            // Warm every resident pose before drawing its real retained image.
            glDisable(GL_SCISSOR_TEST);glDisable(GL_BLEND);glDisable(GL_CULL_FACE);
            glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LESS);glDepthMask(GL_TRUE);
            glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);prepare_reflected_foliage();
            WorldWildlifePrepareFrame prep;
            prep.program=foliage.id;prep.pass=1;prep.x=mirror.x;prep.y=mirror.y;prep.z=mirror.z;
            prep.yaw=mirror.yaw;prep.pitch=mirror.pitch;prep.loading=true;
            prep.constrain_minimum_y=true;prep.minimum_y=double(water_level)+.01;
            if(!world_wildlife_prepare(wildlife,prep))return false;
        }
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        glDisable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);glDisable(GL_CULL_FACE);
        if(record_stats&&profile_gpu)sky_reflection_queries.begin(frame);
        WorldSkyFrame reflected_sky=sky_frame(mirror,light,mirror_right,mirror_up,mirror_forward,0,0);reflected_sky.volumetric_clouds=graphics.clouds;
        const auto sky_before=world_sky_stats(sky);
        if(!world_sky_draw(sky,reflected_sky,reflection.fbo,reflection.w,reflection.h,1)) {
            return false;
        }
        if(record_stats)sky_reflection_play_allocations+=world_sky_stats(sky).target_allocations-sky_before.target_allocations;
        if(record_stats&&profile_gpu)sky_reflection_queries.end();
        // Only the pool samples this raised reflection. Keep its complete
        // rectangular footprint, maximum shader distortion/blur and filtering
        // guard. Every potentially sampled texel retains the same resolution,
        // scene, shading and sky; omit geometry rasterization elsewhere.
        const auto *reflection_pool=world_atlas_rainforest_pool();
        if(builtin_pool_only&&water_level==float(reflection_pool->surface_y)) {
            WorldPoolReflectionScissorInput input;
            std::memcpy(input.vp,reflection_vp.m,sizeof(input.vp));
            input.pool_bounds[0]=float(reflection_pool->x);input.pool_bounds[1]=float(reflection_pool->z);
            input.pool_bounds[2]=float(1/reflection_pool->radius_x);input.pool_bounds[3]=float(1/reflection_pool->radius_z);
            input.origin[0]=float(cam.x);input.origin[1]=float(cam.z);
            input.offset_y=float(-cam.y);input.water_level=water_level;
            input.reflection_shift_y=float(2*(cam.y-reflection_pool->surface_y));
            input.width=reflection.w;input.height=reflection.h;
            const auto region=world_pool_reflection_scissor(input);
            if(pool_reflection_scissor_enabled&&region.restricted) {
                glEnable(GL_SCISSOR_TEST);glScissor(region.x,region.y,region.width,region.height);
                if(record_stats)pool_reflection_scissor_frames++;
            }
            if(record_stats) {
                pool_reflection_full_pixels+=uint64_t(reflection.w)*reflection.h;
                pool_reflection_geometry_pixels+=pool_reflection_scissor_enabled&&region.restricted?
                    uint64_t(region.width)*region.height:uint64_t(reflection.w)*reflection.h;
            }
        }
        // Only certified sole-river consumers use this additional geometry
        // restriction. Sky above was rendered completely, at the same resolution.
        if(record_stats&&sole_river_consumer&&river_main_vp) {
            WorldRiverReflectionInput input;std::memcpy(input.main_vp,river_main_vp->m,sizeof(input.main_vp));
            std::memcpy(input.reflection_vp,reflection_vp.m,sizeof(input.reflection_vp));
            input.origin[0]=float(cam.x);input.origin[1]=float(cam.z);input.offset_y=float(-cam.y);
            input.shift_y=float(2*(cam.y-water_level));input.level=water_level;input.width=reflection.w;input.height=reflection.h;
            WorldRiverReflectionSupport support(input);const auto &rivers=studio_surface_gpu.rivers;
            for(const auto&r:rivers.ranges)if(WorldSurfaceRivers::nearby(r,cam.x,cam.z,studio.open?4096.:1200.)) {
                if(r.first<0||r.count<0||r.count%3||size_t(r.first)+size_t(r.count)>rivers.support_vertices.size()){support.fail_open();break;}
                if(support.outside_range(r.bounds[0],r.bounds[1],r.bounds[2],r.bounds[3],r.ymin,r.ymax))continue;
                for(int k=r.first;k<r.first+r.count;k+=3)support.triangle(rivers.support_vertices[size_t(k)],rivers.support_vertices[size_t(k+1)],rivers.support_vertices[size_t(k+2)]);
            }
            const auto region=support.region();++river_reflection_support_frames;
            river_reflection_full_pixels+=uint64_t(reflection.w)*reflection.h;
            river_reflection_geometry_pixels+=river_reflection_scissor_enabled&&region.restricted?uint64_t(region.width)*region.height:uint64_t(reflection.w)*reflection.h;
            if(river_reflection_scissor_enabled&&region.restricted){glEnable(GL_SCISSOR_TEST);glScissor(region.x,region.y,region.width,region.height);++river_reflection_restricted_frames;}
        }
        glEnable(GL_DEPTH_TEST);glDepthMask(GL_TRUE);glEnable(GL_CULL_FACE);
        land.use();light_program(land,light,mirror);torch_program(land,torch_lights);bind_local_probes(land,mirror);land.v2("uWorldSize",float(world_atlas_width()),float(world_atlas_depth()));
        land.v3("uEye",{float(mirror.x),float(mirror.y),float(mirror.z)});land.m4("uLightVP",reflected_light);
        land.i("uClipBelowWater",1);land.f("uWaterClipLevel",water_level);
        weather_program(land,wind_x,wind_z,wetness);
        const char *ref_names[]={"uGrass","uRock","uSand","uDirt","uGrassNormal","uRockNormal","uClimate","uShadow","uGrassARM","uRockARM"};
        for(int i=0;i<10;i++) {glActiveTexture(GL_TEXTURE0+i);glBindTexture(GL_TEXTURE_2D,i==7?shadow.depth:materials[i]);land.i(ref_names[i],i);}
        glActiveTexture(GL_TEXTURE11);glBindTexture(GL_TEXTURE_2D,macro_height);land.i("uMacroHeight",11);
        landscape.bind(land);studio_surface_gpu.bind(land,cam.x,cam.y,cam.z);studio.surface_terrain_binding(land);
        showcase_terrain.bind(land.id);
        terrain.draw(land,mirror,reflection_vp);
        glDisable(GL_CULL_FACE);
        prepare_reflected_foliage();
        // Exterior water can reflect woodland as well as architecture.
        // Keep the existing bounded tree proxies and distant scenery.
        {
            // AAA merge: reflected vegetation/scenery use the hair-free variant.
            Program &reflected_vegetation=(lean_vegetation_ok()&&foliage.vegetation?*foliage.vegetation:foliage);
            if(&reflected_vegetation!=&foliage)prepare_reflected_foliage_program(reflected_vegetation);
            dressing.draw_reflected(reflected_vegetation,mirror);horizon.draw(reflected_vegetation,mirror);outcrops.draw(reflected_vegetation,mirror);
            reflected_vegetation.i("uSourceMaterialAO",city_showcase?1:0);
            world_scenery_draw(scenery,reflected_vegetation.id,mirror.x,mirror.y,mirror.z);
            reflected_vegetation.i("uSourceMaterialAO",0);
            foliage.use();
        }
        structure_gpu.draw(mirror,reflection_vp,reflected_light,light.sun,light.sunColor,light.ambient,light.fog,light.haze,shadow.depth,shadow.side,false,water_level-mirror.y);
        foliage.use();
        if(world_village_population_stats(population).visible){prepare_reflected_foliage_program(live_programs.crowd);draw_population_color(live_programs.crowd,mirror,reflection_vp);foliage.use();}
        if((world_road_traffic_count(road_traffic)>0)){prepare_reflected_foliage_program(live_programs.crowd);world_road_traffic_draw(road_traffic,live_programs.crowd.id,mirror.x,mirror.y,mirror.z,mirror.yaw,mirror.pitch,mirror.aspect,1,reflection_vp.m);foliage.use();}
        world_castle_training_draw(training,foliage.id,mirror.x,mirror.y,mirror.z);
        WorldTorchLights reflected_actor_lights;
        if(!source_actor_practicals(mirror,visual_time,reflected_actor_lights))return false;
        if(!cam.fly){
            if(source_flames_enabled){torch_program(foliage,reflected_actor_lights);foliage.i("uActorLighting",1);}
            world_combat_motion_draw(combat_motion,foliage.id,mirror.x,mirror.y,mirror.z,false,!story_civilian_ready&&!skeletal_hero_active&&!arena_player_bank);
            if(battlefield_combat_motion){world_combat_motion_draw(battlefield_combat_motion,foliage.id,mirror.x,mirror.y,mirror.z,false,false);world_combat_motion_draw_crowd(battlefield_combat_motion,foliage.id,mirror.x,mirror.y,mirror.z,false);}
            if(arena_compact_roster_enabled)arena_each_appearance([&](auto *motion){world_combat_motion_draw(motion,foliage.id,mirror.x,mirror.y,mirror.z,false,!skeletal_hero_active);});
            else if(arena_enabled&&arena_opponent_motion()!=combat_motion)world_combat_motion_draw(arena_opponent_motion(),foliage.id,mirror.x,mirror.y,mirror.z,false,false);
            if(source_flames_enabled){foliage.i("uActorLighting",0);torch_program(foliage,torch_lights);}
        }
        world_village_watch_draw(watch,foliage.id,mirror.x,mirror.y,mirror.z);
        world_village_pets_draw(pets,foliage.id,mirror.x,mirror.y,mirror.z);
        world_wildlife_draw(wildlife,foliage.id,mirror.x,mirror.y,mirror.z,mirror.yaw,mirror.pitch,mirror.aspect,1);
        world_bandit_camp_draw(bandits,foliage.id,mirror.x,mirror.y,mirror.z,mirror.yaw,mirror.pitch,mirror.aspect,1);
        if(source_flames_enabled)torch_program(foliage,reflected_actor_lights);
        world_stronghold_draw(stronghold,foliage.id,mirror.x,mirror.y,mirror.z,1,reflection_vp.m);
        if(source_flames_enabled)torch_program(foliage,torch_lights);
        world_trail_camp_draw(trail_camp,foliage.id,mirror.x,mirror.y,mirror.z,mirror.yaw,mirror.pitch,mirror.aspect,1);
        world_trail_camp_draw(arrival_camp,foliage.id,mirror.x,mirror.y,mirror.z,mirror.yaw,mirror.pitch,mirror.aspect,1);
        world_trail_camp_draw(first_journey_townsfolk,foliage.id,mirror.x,mirror.y,mirror.z,mirror.yaw,mirror.pitch,mirror.aspect,1);
        battlefield_draw_arrows(foliage,mirror);
        world_castle_dragons_draw(dragons,foliage.id,mirror.x,mirror.y,mirror.z,mirror.yaw,mirror.pitch,mirror.aspect,1);
        if(!draw_live_color(mirror,reflection_vp,reflected_light,light,torch_lights,visual_time,wetness,true,water_level))return false;
        if(!world_torch_flames_draw(torch_flames,torch_frame(mirror,light,mirror_right,mirror_up,mirror_forward,
                reflection_vp,reflection.fbo,reflection.w,reflection.h,true,water_level))) {
            glDisable(GL_SCISSOR_TEST);
            if(record_stats)torch_failures++;
            return false;
        }
        if(dragon_breath){
            const auto f=dragon_breath_frame(mirror,light,mirror_right,mirror_up,mirror_forward,reflection_vp,
                reflection.color,reflection.depth,reflection.w,reflection.h,visual_time,wind_x,wind_z);
            if(!world_dragon_breath_draw(dragon_breath,f))return false;
        }
        if(record_stats)torch_reflection_frames++;
        glDisable(GL_SCISSOR_TEST);
        glDrawBuffer(GL_COLOR_ATTACHMENT0);glDepthMask(GL_TRUE);
        return true;
    };
    auto bind_water_style=[&](unsigned source,const world_studio::Water *custom,float time)->bool{
        if(studio_imported_features){studio_surface_gpu.bind(water,cam.x,cam.y,cam.z);glActiveTexture(GL_TEXTURE12);glBindTexture(GL_TEXTURE_2D_ARRAY,studio_surface_gpu.choose(landscape.masks));water.i("uSurfaceTextures",12);}const char *targets[]={"__water_ocean__","__water_pool__","__water_moat__","__water_pond__"};studio.surface_graph(water,targets[std::min(source,3u)]);
        if(!studio_water_features)return true;
        const world_studio::Water *style=world_water_source_style(studio.live,source,custom);
        if(!world_water_style_enabled(studio.live,style))return false;
        water.v3("uStudioWaterTint",style?V3{style->tint[0],style->tint[1],style->tint[2]}:V3{1,1,1});water.v3("uStudioWaterAbsorption",style?V3{style->absorption[0],style->absorption[1],style->absorption[2]}:V3{1,1,1});glUniform4f(water.at("uStudioWaterParams"),style?style->roughness:1,style?style->waves:1,style?style->flow:1,style?style->foam:1);water.f("uTime",time*(style?style->flow:1));return true;
    };
    std::array<std::uint64_t,8> studio_pond_draws={},studio_pond_reflection_draws={};
    std::uint64_t studio_river_draws=0,studio_river_reflection_draws=0;
    auto draw_water_layers=[&](const Camera &cam,const Lighting &light,const WorldTorchLights &torch_lights,
            const M4 &lightVP,const M4 &vp,const M4 &reflection_vp,float visual_time,float water_level,
            float wind_x,float wind_z,float wetness,float rain_intensity,const float *jitter,
            bool reflect_water,bool draw_pool,bool draw_creek,bool record_stats,bool river_reflection=false)->bool {
        const V3 forward=cam.forward(),right=cam.right(),up=norm(cross(right,forward));
        const auto *pool=world_atlas_rainforest_pool();
        const GLenum color_buffers[]={GL_COLOR_ATTACHMENT0,GL_COLOR_ATTACHMENT1};
        const double custom_water_distance=studio.open?4096.:1200.;
        bool custom_water=studio_surface_gpu.rivers.visible(cam.x,cam.z,custom_water_distance);for(const auto &w:studio.live.waters)if(w.source==3&&w.enabled&&world_entity::visible(studio.live.entities,{world_entity::Kind::Water,unsigned(&w-studio.live.waters.data())})&&std::hypot(cam.x-w.x,cam.z-w.z)<custom_water_distance)custom_water=true;
        const bool water_scene_ready=(reflect_water||draw_pool||draw_creek||custom_water)&&world_water_scene_capture(water_scene,target.fbo,target.w,target.h);
        if(water_scene_ready&&record_stats) {
            water_scene_frames++;
            if(water_scene_width!=target.w||water_scene_height!=target.h) {
                water_scene_play_allocations++;water_scene_width=target.w;water_scene_height=target.h;
            }
        }
        water_scene_peak_bytes=std::max(water_scene_peak_bytes,world_water_scene_bytes(water_scene));
        glDisable(GL_CULL_FACE);
        glDrawBuffers(2,color_buffers);
        water.use();
        const auto water_detail=world_water_quality_profile(graphics.water_quality);
        water.i("uSurfaceDetail",int(graphics.water_quality));
        water.i("uSSRSteps",graphics.reflections?water_detail.ssr_steps:0);
        water.i("uSSRRefineSteps",water_detail.ssr_refine_steps);
        water.i("uFilteredReflections",water_detail.filtered?1:0);
        light_program(water, light,cam);torch_program(water,torch_lights);
        weather_program(water,wind_x,wind_z,wetness);water.f("uRainIntensity",rain_intensity);
        water.i("uAuthoredPond",0);water.i("uRiverSurface",0);water.i("uCastleCreek",0);water.i("uLocalPool",0);water.f("uWaterLevel",0);
        water.m4("uVP", vp);water.m4("uLightVP",lightVP);water.i("uShadow",7);
        water.v3("uOffset", {0, float(-cam.y), 0});
        water.v2("uOrigin", float(cam.x), float(cam.z));
        glActiveTexture(GL_TEXTURE6);
        glBindTexture(GL_TEXTURE_2D,materials[6]);
        water.i("uClimate",6);
        water.i("uHasOpaque",water_scene_ready?1:0);
        water.v2("uResolution",float(target.w),float(target.h));
        water.v3("uForward",forward);water.v3("uRight",right);water.v3("uUp",up);
        water.f("uEyeHeight",float(cam.y));water.f("uTanHalfFov",std::tan(cam.fov*.5f));
        water.v2("uJitterNDC",2*jitter[0]/target.w,2*jitter[1]/target.h);
        water.f("uNear",view_near_plane());water.f("uFar",15000.f);
        glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,world_water_scene_color(water_scene));water.i("uOpaqueColor",0);
        glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,world_water_scene_depth(water_scene));water.i("uOpaqueDepth",1);
        water.v2("uWorldSize",float(world_atlas_width()),float(world_atlas_depth()));
        glActiveTexture(GL_TEXTURE10);glBindTexture(GL_TEXTURE_2D,reflection.color);
        water.i("uReflection",10);water.i("uHasReflection",reflect_water&&water_level==0?1:0);
        water.m4("uReflectionVP",reflection_vp);water.v3("uReflectionShift",{0,float(2*cam.y),0});
        water.f("uSize", 50000);
        water.f("uTime", visual_time);
        glBindVertexArray(emptyVAO);
        water.i("uAuthoredPond",0);
        if(bind_water_style(0,nullptr,visual_time))glDrawArrays(GL_TRIANGLES, 0, 6);
        if(draw_pool) {
            water.i("uAuthoredPond",0);
            if(record_stats){pool_frames++;if(reflect_water)pool_reflection_frames++;}
            water.i("uLocalPool",1);water.f("uWaterLevel",float(pool->surface_y));
            glUniform4f(water.at("uPoolBounds"),float(pool->x),float(pool->z),float(1/pool->radius_x),float(1/pool->radius_z));
            water.i("uHasReflection",reflect_water&&water_level==float(pool->surface_y)?1:0);
            water.v3("uReflectionShift",{0,float(2*(cam.y-pool->surface_y)),0});
            if(bind_water_style(1,nullptr,visual_time))glDrawArrays(GL_TRIANGLES,0,6);
            water.i("uLocalPool",0);water.f("uWaterLevel",0);
        }
        if(draw_creek&&!city_showcase) {
            water.i("uAuthoredPond",0);
            if(record_stats){creek_frames++;if(reflect_water)creek_reflection_frames++;}
            double ox,oz;world_atlas_village_origin(&ox,&oz);
            water.i("uCastleCreek",1);water.i("uLocalPool",1);water.f("uWaterLevel",float(WORLD_CASTLE_CREEK_SURFACE_Y));
            glUniform4f(water.at("uPoolBounds"),float(ox+(WORLD_CASTLE_CREEK_MIN_X+WORLD_CASTLE_CREEK_MAX_X)*.5),
                float(oz+(WORLD_CASTLE_CREEK_MIN_Z+WORLD_CASTLE_CREEK_MAX_Z)*.5),
                float(2./(WORLD_CASTLE_CREEK_MAX_X-WORLD_CASTLE_CREEK_MIN_X)),
                float(2./(WORLD_CASTLE_CREEK_MAX_Z-WORLD_CASTLE_CREEK_MIN_Z)));
            water.i("uHasReflection",reflect_water&&water_level==float(WORLD_CASTLE_CREEK_SURFACE_Y)?1:0);
            water.v3("uReflectionShift",{0,float(2*(cam.y-WORLD_CASTLE_CREEK_SURFACE_Y)),0});
            if(bind_water_style(2,nullptr,visual_time))glDrawArrays(GL_TRIANGLES,0,6);
            water.i("uCastleCreek",0);water.i("uLocalPool",0);water.f("uWaterLevel",0);
        }
        if(custom_water)for(const auto &w:studio.live.waters)if(w.source==3&&w.enabled&&world_entity::visible(studio.live.entities,{world_entity::Kind::Water,unsigned(&w-studio.live.waters.data())})&&std::hypot(cam.x-w.x,cam.z-w.z)<custom_water_distance){
            water.i("uAuthoredPond",1);
            const bool reflected=reflect_water&&water_level==float(w.level);
            water.i("uLocalPool",1);water.i("uCastleCreek",0);water.i("uHasReflection",reflected?1:0);water.f("uWaterLevel",float(w.level));glUniform4f(water.at("uPoolBounds"),float(w.x),float(w.z),1/w.radius_x,1/w.radius_z);
            water.v3("uReflectionShift",{0,float(2*(cam.y-w.level)),0});
            if(bind_water_style(3,&w,visual_time)){glDrawArrays(GL_TRIANGLES,0,6);if(record_stats){const auto index=unsigned(&w-studio.live.waters.data());++studio_pond_draws[index];if(reflected)++studio_pond_reflection_draws[index];}}
        }
        for(const auto &r:studio_surface_gpu.rivers.ranges)if(WorldSurfaceRivers::nearby(r,cam.x,cam.z,custom_water_distance)){
            const bool reflected=reflect_water&&river_reflection;
            water.i("uAuthoredPond",0);water.i("uRiverSurface",1);water.i("uLocalPool",1);water.i("uCastleCreek",0);water.i("uHasReflection",reflected?1:0);water.f("uWaterLevel",r.level);
            water.f("uRiverReflectionLevel",water_level);water.v3("uReflectionShift",{0,float(2*(cam.y-water_level)),0});
            if(bind_water_style(4,nullptr,visual_time)){glBindVertexArray(studio_surface_gpu.rivers.vao);glDrawArrays(GL_TRIANGLES,r.first,r.count);if(record_stats){++studio_river_draws;if(reflected)++studio_river_reflection_draws;}}
        }
        water.i("uAuthoredPond",0);water.i("uRiverSurface",0);

        water.i("uLocalPool",0);water.i("uCastleCreek",0);water.f("uWaterLevel",0);
        glDrawBuffers(2,color_buffers);

        return water_scene_ready;
    };
    #include "world_main_material_warm.inc"
    // Water can enter the view after one turn or a short district walk. Allocate and exercise its
    // ordinary reflection/cloud/snapshot resources during loading, rather than
    // making that approach pay their first GL allocation and blit. The entire
    // castle district shares these targets; other inland starts stay lazy.
    const bool startup_pool_nearby=std::hypot(cam.x-world_atlas_rainforest_pool()->x,cam.z-world_atlas_rainforest_pool()->z)<220.;
    const world_studio::Water *startup_authored_pond=nullptr;
    double startup_pond_distance=220.;
    for(unsigned i=0;i<studio.live.waters.size();++i){const auto &w=studio.live.waters[i];
        if(w.source!=3||!w.enabled||!world_entity::visible(studio.live.entities,{world_entity::Kind::Water,i}))continue;
        const double distance=std::max(0.,std::hypot(cam.x-w.x,cam.z-w.z)-std::max(double(w.radius_x),double(w.radius_z)));
        if(distance<startup_pond_distance){startup_pond_distance=distance;startup_authored_pond=&w;}
    }
    if(running&&(startup_authored_pond||(!city_showcase&&(castle_creek_nearby(cam)||world_atlas_village_weight(cam.x,cam.z)>0.f))||startup_pool_nearby)) {
        const Uint64 begin=SDL_GetPerformanceCounter();
        bool ready=true;
        const auto water_detail=world_water_quality_profile(graphics.water_quality);
        if(graphics.reflections&&water_detail.planar) {
            const float scale=std::min(graphics.reflection_scale,water_detail.mirror_scale_cap);
            ready=reflection.resize(std::max(1,int(std::lround(target.w*scale))),
                                    std::max(1,int(std::lround(target.h*scale))));
            if(ready) {
                const float visual_time=qa_fixed_time?22.f:17.f;
                const Lighting light=local_lighting(hour,cloud,visual_time,
                    world_atlas_rainforest_weight(cam.x,cam.z),bearing,sun_strength,sky_fill);
                WorldTorchLights torch_lights;
                const double camera[]={cam.x,cam.y,cam.z};
                const float sun[]={light.sun.x,light.sun.y,light.sun.z},center[]={0,-25,0};
                M4 lightVP;
                ready=world_shadow_directional_matrix(camera,sun,160.f,shadow.side,center,lightVP.m)&&
                    world_torch_lights_build(town_torch_emitters.data(),city_showcase?0:town_torch_emitters.size(),
                        creek_origin_x,creek_origin_z,cam.x,cam.y,cam.z,visual_time,torch_lights);
                // Reflected materials sample a complete initialized shadow map.
                glBindFramebuffer(GL_FRAMEBUFFER,shadow.fbo);glDepthMask(GL_TRUE);glClearDepth(1.);
                glClear(GL_DEPTH_BUFFER_BIT);
                for(int direction=0;ready&&direction<4;direction++) {
                    Camera warm=cam;warm.yaw=direction*1.57079632679f;warm.pitch=-.08f;
                    world_scenery_update(scenery,warm.x,warm.y,warm.z,warm.yaw,warm.pitch,warm.aspect,std::tan(warm.fov*.5f),foliage_detail_scale,foliage_density_scale);
                    M4 reflectedVP;
                    const float level=startup_authored_pond?float(startup_authored_pond->level):startup_pool_nearby?float(world_atlas_rainforest_pool()->surface_y):float(WORLD_CASTLE_CREEK_SURFACE_Y);
                    ready=draw_water_reflection(warm,light,torch_lights,lightVP,visual_time,
                        level,0,0,0,qa_fixed_time?0.f:1.f/60.f,reflectedVP,false,0,!startup_authored_pond);
                    if(ready) {
                        glBindFramebuffer(GL_FRAMEBUFFER,target.fbo);glViewport(0,0,target.w,target.h);
                        const GLenum buffers[]={GL_COLOR_ATTACHMENT0,GL_COLOR_ATTACHMENT1};glDrawBuffers(2,buffers);
                        glDepthMask(GL_TRUE);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
                        const M4 vp=product(perspective(warm.fov,warm.aspect,view_near_plane(),15000.f),look({0,0,0},warm.forward(),{0,1,0}));
                        const float jitter[]={0,0};
                        ready=draw_water_layers(warm,light,torch_lights,lightVP,vp,reflectedVP,visual_time,
                            level,0,0,0,0,jitter,true,!startup_authored_pond&&startup_pool_nearby,!startup_authored_pond&&!startup_pool_nearby,false);
                    }
                    if(ready)creek_water_startup_passes++;
                }
                world_scenery_update(scenery,cam.x,cam.y,cam.z,cam.yaw,cam.pitch,cam.aspect,std::tan(cam.fov*.5f),foliage_detail_scale,foliage_density_scale);
                // Main rendering sets these before every normal color draw.
                foliage.use();foliage.i("uWindHistoryEnabled",0);foliage.f("uWindFrameDelta",0);
            }
        } else if(ready)ready=world_water_scene_capture(water_scene,target.fbo,target.w,target.h);
        if(ready) {
            water_scene_width=target.w;water_scene_height=target.h;
            water_scene_peak_bytes=world_water_scene_bytes(water_scene);
            // Complete backing allocation and first use before starting the
            // ordinary frame clock. There is no synchronization in walking.
            glFinish();creek_water_prewarmed=true;
        } else {
            std::fprintf(stderr,"[World] Nearby creek resources failed to initialize\n");
            launch->exit_code=1;running=false;
        }
        glBindFramebuffer(GL_FRAMEBUFFER,target.fbo);glViewport(0,0,target.w,target.h);
        glEnable(GL_DEPTH_TEST);glDepthMask(GL_TRUE);glEnable(GL_CULL_FACE);glDisable(GL_BLEND);
        glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glActiveTexture(GL_TEXTURE0);
        creek_water_prewarm_ms=millis(begin);
        std::printf("[World] Nearby creek resources ready before play: %s, %.3f ms\n",ready?"yes":"no",creek_water_prewarm_ms);
    }
    bool particles_startup_ready=false;
    if(running) {
        particles_startup_ready=world_scene_particles_prepare(scene_particles,target.w,target.h);
        if(particles_startup_ready) {
            // Some distant starts do not need a creek warm pass. Initialize
            // their shadow texture before the particle shader samples it.
            glBindFramebuffer(GL_FRAMEBUFFER,shadow.fbo);glDepthMask(GL_TRUE);glClearDepth(1.);glClear(GL_DEPTH_BUFFER_BIT);
            Camera warm=cam;warm.x=creek_origin_x-138.5;warm.y=35.;warm.z=creek_origin_z-83.;warm.yaw=0;warm.pitch=0;
            const V3 f=warm.forward(),r=norm(cross(f,{0,1,0})),u=cross(r,f);
            const M4 vp=product(perspective(warm.fov,warm.aspect,view_near_plane(),15000.f),look({0,0,0},f,{0,1,0}));
            const Lighting light=local_lighting(hour,cloud,22.f,0,bearing,sun_strength,sky_fill);
            M4 lightvp={};const double eye[]={warm.x,warm.y,warm.z};
            const float sun[]={light.sun.x,light.sun.y,light.sun.z},center[]={0,-25,0};
            particles_startup_ready=world_shadow_directional_matrix(eye,sun,160.f,shadow.side,center,lightvp.m);
            if(particles_startup_ready) {
                const auto f0=scene_particle_frame(warm,light,r,u,f,vp,lightvp,target.w,target.h,target.color,target.depth,shadow.depth,world_environment_effects_profile(graphics.environment_effects),hour,last_wind_x,last_wind_z);
                particles_startup_ready=world_scene_particles_warm(scene_particles,f0);
                glFinish();
            }
        }
        if(!particles_startup_ready){std::fprintf(stderr,"[World] Scene particles failed to warm before play\n");launch->exit_code=1;running=false;}
        glDepthMask(GL_TRUE);
    }
    bool bloom_startup_ready=false;
    size_t bloom_play_frames=0;
    if(running&&bloom_enabled) {
        const GLuint glow=world_bloom_resolve(bloom,target.color,target.w,target.h,exposure*auto_exposure_factor);
        if(!glow) {bloom_failures++;launch->exit_code=1;running=false;}
        else {
            compose_post(target.color,target.depth,glow);glFinish();
            glDepthMask(GL_TRUE);bloom_startup_ready=true;
        }
    }
    if(running&&!warm_main_materials(cam,qa_fixed_time?22.f:17.f,true)) {
        std::fprintf(stderr,"[World] Main material layouts failed to initialize\n");
        launch->exit_code=1;running=false;
    }
    // Audio is optional and owns its mixer independently of the legacy scene.
    // Decode finished PCM before starting the measured walking-frame clock.
    // Hero footsteps have one owner. Eligible visible foot plants (grounded
    // locomotion, not idle/turn replanting) become at most one step event.
    std::uint64_t hero_step_plants=0,hero_steps_played=0,hero_steps_filtered=0;
    double hero_step_x=0,hero_step_z=0;float hero_step_speed=0,hero_step_gap=0;bool hero_step_ready=false;
    // Apartment walking pace (m/s). VALWOOD_APARTMENT_WALK_SPEED=0 restores
    // the ordinary outdoor walk/sprint policy indoors.
    float apartment_walk_speed=1.25f;
    if(const char *v=std::getenv("VALWOOD_APARTMENT_WALK_SPEED")){char *end=nullptr;const float x=std::strtof(v,&end);if(end!=v&&std::isfinite(x))apartment_walk_speed=std::clamp(x,0.f,3.5f);}
    char demo_audio_error[256]={};
    WorldDemoAudio *demo_audio=running?world_demo_audio_create("audio/demo",demo_audio_error,sizeof(demo_audio_error)):nullptr;
    if((combat_motion||joust_enabled)&&!training_sounds.init())std::fprintf(stderr,"[Training] Optional combat sounds unavailable\n");
    arena_shot_sound=[&](){training_sounds.play(0,25);};
    if(presentation){
        if(!preview_audio.init())std::fprintf(stderr,"[Preview] Optional presentation score unavailable\n");
        if(!preview_voices.init())std::fprintf(stderr,"[Preview] Optional recorded dialogue could not initialize\n");
        std::printf("[Preview] Human dialogue recordings available: %u/4\n",preview_voices.available);
    }
    if(world_controller_enabled){
        SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER);
        if(joust_qa||qa_controller||qa_town||opening_qa||npc_intro_qa||road_traffic_qa||qa_stick_combat||arena_qa||qa_grass||qa_minimap||qa_walking_controls||equipment_qa){
            if(!controller_test_pad.attach()){std::fprintf(stderr,"[Controller QA] Virtual device failed: %s\n",SDL_GetError());launch->exit_code=1;running=false;}
            else preview_pad.prefer(SDL_JoystickInstanceID(controller_test_pad.joystick));
        }else preview_pad.open();
        preview_sync_controller_ui();
    }
    if(presentation){studio.components_show_dialogue=false;show_tools=false;journey_open=false;}
    size_t demo_audio_fire_sources=0;
    uint64_t demo_audio_update_failures=0;
    if(demo_audio) {
        std::array<WorldDemoAudioFire,WORLD_DEMO_AUDIO_FIRE_CAPACITY> audio_fires={};
        static_assert(WORLD_TORCH_FLAME_CAPACITY<=WORLD_DEMO_AUDIO_FIRE_CAPACITY,"Audio follows the bounded real flame set");
        for(size_t i=0;i<flame_emitters.size();i++) {
            const auto &e=flame_emitters[i];
            audio_fires[i]={e.x,e.y,e.z,std::clamp(10.f+16.f*e.width,10.f,22.f),
                std::clamp(e.width/.35f,.12f,1.f)*std::clamp(e.intensity,0.f,1.f)};
        }
        size_t audio_fire_count=flame_emitters.size();
        for(const auto &source:studio.live.particles)if(source.enabled&&source.source<0&&source.kind==2&&audio_fire_count<audio_fires.size()){
            audio_fires[audio_fire_count++]={source.x,source.y,source.z,std::clamp(10.f+16.f*source.radius,10.f,22.f),std::clamp(source.power,0.f,1.f)};
        }
        if(world_demo_audio_set_fires(demo_audio,audio_fires.data(),audio_fire_count))
            demo_audio_fire_sources=audio_fire_count;
        else {
            std::fprintf(stderr,"[World] Optional demo audio disabled: invalid shared fire origins\n");
            training_sounds.release();
    world_demo_audio_free(demo_audio);demo_audio=nullptr;
        }
    }else if(running)std::fprintf(stderr,"[World] Optional demo audio unavailable: %s\n",demo_audio_error);
    #include "world_weather_runtime.inc"
    std::function<bool(std::string&)> cutscene_sound_reprepare;
    std::function<bool()> cutscene_sound_uses_library;
    #include "world_soundscape_runtime.inc"
    #include "world_cutscenes_runtime.inc"
    #include "world_joust_runtime.inc"
    #include "world_joust_qa.inc"
    #include "world_ultra4k_ui_qa.inc"
    Uint64 last = SDL_GetPerformanceCounter();
    uint64_t frame = 0;
    #include "world_runtime_live_qa.inc"
    #include "world_presentation_qa.inc"
    #include "world_milestone_qa.inc"
    #include "world_controller_qa.inc"
    #include "world_town_qa.inc"
    #include "world_opening_prologue_qa.inc"
    #include "world_npc_intro_qa.inc"
    #include "world_road_traffic_qa.inc"
    #include "world_first_journey_qa.inc"
    #include "world_grass_interaction_qa.inc"
    unsigned minimap_qa_phase=0,minimap_qa_checks=0,minimap_qa_captures=0,minimap_qa_pending=0,minimap_qa_first_gl_error=0;
    bool minimap_qa_passed=false,minimap_qa_failed=false;double minimap_qa_age=0,minimap_qa_x=0,minimap_qa_z=0,minimap_qa_yaw=0,minimap_qa_travel=0,minimap_qa_turn=0;
    uint64_t minimap_qa_queries=0;
    std::array<std::array<double,11>,3> minimap_qa_observations{};
    auto minimap_check=[&](bool condition,const char *message){++minimap_qa_checks;if(condition)return true;minimap_qa_failed=true;launch->exit_code=1;running=false;controller_test_pad.axis(SDL_CONTROLLER_AXIS_LEFTY,0);controller_test_pad.axis(SDL_CONTROLLER_AXIS_RIGHTX,0);std::fprintf(stderr,"[Minimap QA] FAIL phase%u: %s\n",minimap_qa_phase,message);return false;};
    auto minimap_workflow=[&](float delta){
        if(!qa_minimap||minimap_qa_passed||minimap_qa_failed)return;
        controller_test_pad.tick();
        if(frame>uint64_t(launch->frames)-10){minimap_check(false,"Frame budget exhausted");return;}
        if(destination_loading||frame<=uint64_t(launch->warmup)||!terrain.view_ready(cam.x,cam.z,false)||world_scenery_streaming_stats(scenery).critical_pending)return;
        if(minimap_qa_phase==0){
            SDL_RaiseWindow(window);SDL_SetWindowInputFocus(window);SDL_PumpEvents();preview_pad.focused=true;
            if(!minimap_check(minimap_valid&&minimap_config.enabled&&!launch->no_hud&&world_minimap::in_region(minimap_config,{cam.x,cam.z}),"Valid opt-in HUD config and player in region"))return;
            if(!minimap_check(!source_review.active&&!cam.fly&&cam.grounded&&!studio.open&&!studio.cinematic_preview&&!preview_blocked(),"Grounded ordinary gameplay"))return;
            if(!minimap_check(preview_pad.id()==SDL_JoystickInstanceID(controller_test_pad.joystick),"Owned actual SDL virtual controller"))return;
            if(!minimap_check(active_playtest_walk||active_city_physics,"Production source collision available"))return;
            minimap_qa_x=cam.x;minimap_qa_z=cam.z;minimap_qa_yaw=cam.yaw;minimap_qa_queries=grass_player_movement_queries;minimap_qa_phase=1;minimap_qa_age=0;return;
        }
        if(!minimap_check(!cam.fly&&cam.grounded&&!studio.open&&!studio.cinematic_preview&&!preview_blocked(),"Controller remains in grounded gameplay"))return;
        minimap_qa_age+=delta;
        if(minimap_qa_phase==1&&minimap_qa_age>=.5)minimap_qa_pending=1;
        if(minimap_qa_phase==2&&minimap_qa_age>=1.){
            controller_test_pad.axis(SDL_CONTROLLER_AXIS_LEFTY,0);controller_test_pad.tick();
            minimap_qa_travel=std::hypot(cam.x-minimap_qa_x,cam.z-minimap_qa_z);
            if(!minimap_check(minimap_qa_travel>=.5&&minimap_qa_travel<=4.,"Actual SDL movement updates map player"))return;
            if(!minimap_check(grass_player_movement_queries>minimap_qa_queries+5&&grass_player_movement_query_failures==0,"Ordinary source movement queries"))return;
            minimap_qa_pending=2;minimap_qa_phase=3;
        }
        if(minimap_qa_phase==4&&minimap_qa_age>=.65){
            controller_test_pad.axis(SDL_CONTROLLER_AXIS_RIGHTX,0);controller_test_pad.tick();minimap_qa_turn=std::remainder(double(cam.yaw)-minimap_qa_yaw,world_minimap::pi*2);
            if(!minimap_check(std::abs(minimap_qa_turn)>.2&&std::abs(minimap_qa_turn)<2.,"Actual right stick updates north-up facing arrow"))return;
            minimap_qa_pending=3;minimap_qa_phase=5;
        }
    };
    auto minimap_capture=[&](){
        if(!qa_minimap||!minimap_qa_pending)return;
        if(!minimap_check(minimap_drawn&&minimap_stats.primitives>10&&minimap_stats.primitives<=world_minimap::primitive_limit&&minimap_stats.cache_failures==0&&minimap_geometry.truncated==0,"Bounded live HUD rendered"))return;
        const unsigned index=minimap_qa_pending-1;const char *names[]={"minimap-start.bmp","minimap-moved.bmp","minimap-turned.bmp"};
        std::vector<unsigned char> pixels(size_t(display_w)*display_h*3);GLint alignment=4;glGetIntegerv(GL_PACK_ALIGNMENT,&alignment);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadBuffer(GL_BACK);glReadPixels(0,0,display_w,display_h,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());glPixelStorei(GL_PACK_ALIGNMENT,alignment);
        const size_t stride=size_t(display_w)*3;for(int row=0;row<display_h/2;++row)std::swap_ranges(pixels.data()+row*stride,pixels.data()+(row+1)*stride,pixels.data()+(display_h-1-row)*stride);
        SDL_Surface *surface=SDL_CreateRGBSurfaceWithFormatFrom(pixels.data(),display_w,display_h,24,int(stride),SDL_PIXELFORMAT_RGB24);const bool saved=surface&&SDL_SaveBMP(surface,names[index])==0;if(surface)SDL_FreeSurface(surface);
        const GLenum error=glGetError();if(error&&!minimap_qa_first_gl_error)minimap_qa_first_gl_error=error;
        if(!minimap_check(saved&&error==GL_NO_ERROR,"Actual post-HUD capture"))return;
        world_minimap::Point q;world_minimap::project({cam.x,cam.z},{cam.x,cam.z},minimap_config.radius,q);
        minimap_qa_observations[index]={cam.x,cam.y-1.75,cam.z,cam.yaw,minimap_stats.center_x,minimap_stats.center_y,minimap_stats.half_size,minimap_stats.heading_x,minimap_stats.heading_y,minimap_stats.center_x+q.x*minimap_stats.half_size,minimap_stats.center_y+q.z*minimap_stats.half_size};
        ++minimap_qa_captures;minimap_qa_pending=0;minimap_qa_age=0;
        if(index==0){controller_test_pad.axis(SDL_CONTROLLER_AXIS_LEFTY,-18000);minimap_qa_phase=2;}
        else if(index==1){controller_test_pad.axis(SDL_CONTROLLER_AXIS_RIGHTX,21000);minimap_qa_phase=4;}
        else {minimap_qa_phase=100;minimap_qa_passed=true;running=false;}
    };
    auto minimap_report=[&](){
        if(!qa_minimap)return;
        if(!minimap_qa_passed){minimap_qa_failed=true;launch->exit_code=1;}
        FILE *f=std::fopen("minimap-qa.json","wb");if(!f){launch->exit_code=1;return;}
        std::fprintf(f,"{\"schema\":\"valwood.minimap.runtime-qa.v1\",\"passed\":%s,\"failed\":%s,\"phase\":%u,\"checks\":%u,\"first_gl_error\":%u,\"actual_sdl_controller\":true,\"injected_player_pose\":false,\"captures\":%u,\"travel_m\":%.8f,\"yaw_delta\":%.8f,\"movement_queries\":%llu,\"movement_query_failures\":%llu,\"world_radius_m\":%.8f,\"visible_frames\":%llu,\"cache_builds\":%llu,\"peak_primitives\":%u,\"observations\":[",
            minimap_qa_passed?"true":"false",minimap_qa_failed?"true":"false",minimap_qa_phase,minimap_qa_checks,minimap_qa_first_gl_error,minimap_qa_captures,minimap_qa_travel,minimap_qa_turn,(unsigned long long)(grass_player_movement_queries-minimap_qa_queries),(unsigned long long)grass_player_movement_query_failures,minimap_config.radius,(unsigned long long)minimap_stats.visible_frames,(unsigned long long)minimap_stats.cache_builds,minimap_stats.peak_primitives);
        for(unsigned i=0;i<3;++i){const auto &o=minimap_qa_observations[i];std::fprintf(f,"%s{\"feet\":[%.8f,%.8f,%.8f],\"yaw\":%.9f,\"center\":[%.6f,%.6f],\"half_size_pixels\":%.6f,\"heading_tip\":[%.6f,%.6f],\"projected_player\":[%.6f,%.6f]}",i?",":"",o[0],o[1],o[2],o[3],o[4],o[5],o[6],o[7],o[8],o[9],o[10]);}
        std::fprintf(f,"]}\n");if(std::fclose(f))launch->exit_code=1;
    };

    FoliageCaptureSequence foliage_sequence;
    // Optional, observation-only trace for an ordinary private desktop review.
    // It does not enable a QA driver, change clocks, or inject focus/controls.
    std::ofstream third_person_observation;
    if(const char *path=std::getenv("VALWOOD_OBSERVE_THIRD_PERSON")){
        if(std::filesystem::path(path).is_absolute()&&std::filesystem::exists("PRIVATE_CAPTURE_PROJECT.json")){
            third_person_observation.open(path);third_person_observation<<std::setprecision(12);
        }
    }
    FoliageCaptureSequence npc_sequence{std::getenv("VALWOOD_NPC_SEQUENCE"),true,360,{}};
    double benchmark_x = cam.x, benchmark_z = cam.z;
    float benchmark_yaw = cam.yaw;
    double startup_ms = millis(startup);
    world_model_set_load_observer(nullptr,nullptr);startup_plate.enabled=false;
    size_t peak_grass = 0, peak_trees = 0, loading_frames = 0, loading_streak = 0, longest_loading_streak = 0;
    size_t peak_near=0,peak_mid=0,peak_local_cards=0,peak_horizon=0,peak_horizon_roots=0,peak_scanned_grass=0;
    size_t manual_collision_steps=0,manual_slope_steps=0,manual_sliding_steps=0;
    #include "world_walking_controls_qa.inc"
    const bool qa_demo_tour=launch->frames&&std::getenv("VALWOOD_QA_DEMO_TOUR");
    const bool qa_map_cycle=launch->frames&&!launch->benchmark&&std::getenv("VALWOOD_QA_ATLAS_MAP_CYCLE");
    unsigned atlas_map_qa_events=0;
    unsigned demo_tour_steps=0;
    auto next_demo_stop=[&]() {
        if(source_showcase&&!source_views.empty()){
            source_view_index=(source_view_index+1)%source_views.size();const auto &v=source_views[source_view_index];apply_source_view(v);
            current_name=v.name.c_str();look_active=false;SDL_SetRelativeMouseMode(SDL_FALSE);show_map=false;show_graphics=false;destination_loading=true;temporal_cut=true;
            benchmark_x=cam.x;benchmark_z=cam.z;benchmark_yaw=cam.yaw;demo_tour_steps++;return;
        }
        demo_showcase_index=(demo_showcase_index+1)%std::size(demo_tour);
        look_active=false;SDL_SetRelativeMouseMode(SDL_FALSE);show_map=false;show_graphics=false;
        for(unsigned index=0;index<std::size(scenic_views);++index)if(!std::strcmp(scenic_views[index].id,demo_tour[demo_showcase_index])) {
            scenic_travel(cam,index);current_name=scenic_views[index].name;destination_loading=true;temporal_cut=true;
            benchmark_x=cam.x;benchmark_z=cam.z;benchmark_yaw=cam.yaw;demo_tour_steps++;break;
        }
    };
    if(force_capture_first)third_person=false; // Apply after arena destination setup.
    std::printf("[World] Initial camera %s; V switches first/third person.\n",third_person?"third person":"first person");
    bool rig_capture_started=false; // Bounded private first-person render fixture.
    #include "world_equipment_qa.inc"
    #include "world_third_person_duel_qa.inc"
    launch->active = true;
    const bool qa_studio=launch->frames&&studio.path!="world.studio"&&std::getenv("VALWOOD_QA_STUDIO_WORKFLOW");
    const bool qa_studio_tools=qa_studio&&std::getenv("VALWOOD_QA_STUDIO_TOOLS");
    const bool qa_runtime=launch->frames&&studio.path!="world.studio"&&std::getenv("VALWOOD_QA_RUNTIME_EDITORS");unsigned qa_runtime_phase=0;double qa_pond_x=0,qa_pond_z=0;
    #include "engine/world_studio_surface_qa.inc"
    const bool qa_production=launch->frames&&studio.path.rfind("/tmp/",0)==0&&std::getenv("VALWOOD_QA_PRODUCTION_WORKFLOW");
    unsigned qa_production_phase=0,qa_production_checks=0;bool qa_production_failed=false;
    unsigned qa_studio_phase=0;double qa_studio_x=0,qa_studio_z=0;float qa_studio_expected=0;
    unsigned trail_camp_audit_samples=0,trail_camp_path_audit_samples=0;
    if(const char *audit=std::getenv("VALWOOD_QA_VALIDATE_TRAIL_CAMP"))if(!std::strcmp(audit,"1")&&world_trail_camp_stats(trail_camp).active){
        for(std::size_t i=1;i<std::size(world_trail_camp_approach)&&running;i++){
            const auto *a=world_trail_camp_approach[i-1],*b=world_trail_camp_approach[i];
            const double dx=b[0]-a[0],dz=b[1]-a[1],length=std::hypot(dx,dz);
            const unsigned steps=unsigned(std::ceil(length/.15));
            for(double side:{-.5,0.,.5}){
                double previous_x=a[0]-dz/length*side,previous_z=a[1]+dx/length*side;
                for(unsigned sample=0;sample<=steps;sample++){
                    const double t=double(sample)/steps,x=a[0]+dx*t-dz/length*side,z=a[1]+dz*t+dx/length*side;
                    const float ground=world_atlas_height(x,z);
                    if(!std::isfinite(ground)||world_atlas_local_water(x,z,nullptr,nullptr)||!world_scenery_can_move(scenery,previous_x,previous_z,x,z,ground+.015f,1.75f,.37f)){
                        std::fprintf(stderr,"[TrailCamp path audit] FAIL segment%zu lane%.1f sample%u at%.3f %.3f\n",i,side,sample,x,z);
                        launch->exit_code=1;running=false;break;
                    }
                    previous_x=x;previous_z=z;trail_camp_path_audit_samples++;
                }
                if(!running)break;
            }
        }
        for(unsigned i=0;i<3600&&running;i++){
            const float time=float(i)/30.f;
            world_trail_camp_update(trail_camp,scenery,time);
            world_wildlife_update(wildlife,scenery,time);
            const auto camp=world_trail_camp_stats(trail_camp);const auto animals=world_wildlife_stats(wildlife);
            bool separated=true;
            for(unsigned c=0;c<camp.actors;c++)for(std::size_t a=0;a<animals.actors;a++){
                WorldTrailCamper person;WorldWildlifeActor animal;
                if(!world_trail_camp_actor(trail_camp,c,&person)||!world_wildlife_actor(wildlife,a,&animal)){separated=false;continue;}
                if(animal.species<0||std::size_t(animal.species)>=std::size(kWildlifeSpecies)){separated=false;continue;}
                if(!kWildlifeSpecies[animal.species].airborne&&std::hypot(person.x-animal.x,person.z-animal.z)<kWildlifeSpecies[animal.species].bound_radius+.6)separated=false;
            }
            if(camp.actors!=4||camp.walking!=1||camp.seated!=2||camp.unsafe_roots||animals.actors!=(jungle_expanded?12u:4u)||(!jungle_expanded&&animals.walking!=2)||animals.unsafe_roots||!separated){
                std::fprintf(stderr,"[TrailCamp audit] FAIL at%.3fs: campers%u/walking%u/seated%u unsafe%llu, wildlife unsafe%llu\n",time,camp.actors,camp.walking,camp.seated,(unsigned long long)camp.unsafe_roots,(unsigned long long)animals.unsafe_roots);
                launch->exit_code=1;running=false;break;
            }
            trail_camp_audit_samples++;
        }
        world_trail_camp_update(trail_camp,scenery,17.f);world_wildlife_update(wildlife,scenery,17.f);
        std::printf("[TrailCamp audit] %u samples of actual terrain/collision/animation support before measured play\n",trail_camp_audit_samples);
        std::printf("[TrailCamp path audit] %u actual collision samples across three walking lanes\n",trail_camp_path_audit_samples);
    }
    const int initial_swap_interval=SDL_GL_GetSwapInterval();
    int actual_swap_interval=initial_swap_interval;
    const char *qa_disable_completion=std::getenv("VALWOOD_QA_DISABLE_PRESENT_COMPLETION");
    // Paired native presentation captures keep VSync but remove the extra full
    // GPU drain: 1% low 48.8 -> 59.3 FPS on the same fixed-quality route.
    bool complete_before_swap=actual_swap_interval>0&&!presentation&&!(launch->frames&&qa_disable_completion&&
        !std::strcmp(qa_disable_completion,"1"));
    uint64_t completed_presentation_frames=0;double completion_wait_ms=0;
    double presentation_origin_x,presentation_origin_z;
    world_atlas_village_origin(&presentation_origin_x,&presentation_origin_z);
    bool roost_presentation=false;uint64_t roost_completion_frames=0;
    SDL_PumpEvents();relative_mouse_compatibility.warm(window);
    #include "world_frame_phase_profile.inc"
    engine_perf_begin(launch->profile, launch->warmup);
    engine_perf_set_gpu_stage_timing_available(profile_gpu);
    while (running && !launch->finished) {
        // QA advances simulation at 60 Hz. Pace only its owner thread as well,
        // so uncapped loading frames leave the asset worker real wall time.
        // Ordinary play and all replay assertions/bounds remain unchanged.
        if(opening_qa||npc_intro_qa||road_traffic_qa){
            const Uint64 frequency=SDL_GetPerformanceFrequency();
            const Uint64 interval=(frequency+59)/60;
            Uint64 now=SDL_GetPerformanceCounter();
            while(now-last<interval){
                const Uint64 remaining=interval-(now-last);
                SDL_Delay(Uint32((remaining*1000+frequency-1)/frequency));
                now=SDL_GetPerformanceCounter();
            }
        }
        studio.refresh_scene(scenery);studio.sync_preview(scenery);
        if((playtest_enabled||opening_enabled)&&studio.open){std::fprintf(stderr,"[Playtest] Authoring is unavailable in a shared-asset playtest copy.\n");launch->exit_code=1;running=false;break;}
        Uint64 frame_start = SDL_GetPerformanceCounter();
        const double quality_dt=double(frame_start-last)/SDL_GetPerformanceFrequency();
        float dt = (opening_qa||npc_intro_qa||road_traffic_qa||animation_polish_qa||equipment_qa||duel_qa)?1.f/60.f:std::min(float(quality_dt), .05f);
        last = frame_start;
        if (quality_dt > .00001)
            fps = fps * .94f + float(.06 / quality_dt);
        frame++;
        bool source_review_waiting_frame=false;
        if(studio.designer_exit_approved){running=false;break;}
        sync_designer_camera();if(studio.designer_play_spawn_pending&&!studio.open){
            studio.designer_play_spawn_pending=false;const auto at=studio.designer_play_spawn;
            cam.x=at.x;cam.z=at.z;const double ground=world_atlas_height(at.x,at.z);
            cam.y=(at.y>ground+8?at.y:ground)+1.75;
            if(active_playtest_walk&&active_playtest_walk->loaded()){
                bool found=false;for(double radius:{0.,2.,4.,7.}){for(int step=0;step<(radius==0?1:12);++step){
                    const double angle=step*6.283185307179586/12,px=at.x+std::cos(angle)*radius,pz=at.z+std::sin(angle)*radius;
                    float floor=0;if(active_playtest_walk->surface(px,pz,at.y,floor,3)&&active_playtest_walk->standing_clear(px,pz,floor)){
                        cam.x=px;cam.z=pz;cam.y=floor+1.75;found=true;break;}
                }if(found)break;}
            }
            cam.vy=0;cam.fly=false;cam.grounded=false;
            temporal_cut=true;look_active=false;SDL_SetRelativeMouseMode(SDL_FALSE);
        }refresh_designer_view();
        const bool preview_menu_input=presentation&&world_controller::ui_owns(preview_controller_context());
        if(studio.open||preview_menu_input)ImGui::GetIO().ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard;
        else ImGui::GetIO().ConfigFlags&=~ImGuiConfigFlags_NavEnableKeyboard;
        if(preview_menu_input&&preview_pad.focused)ImGui::GetIO().ConfigFlags|=ImGuiConfigFlags_NavEnableGamepad;
        else ImGui::GetIO().ConfigFlags&=~ImGuiConfigFlags_NavEnableGamepad;
        // Studio can replace the runtime during the preceding UI frame.
        // Refresh before any input/route capsule query can dereference it.
        active_world_runtime=studio.components_ready?studio.components_runtime.get():nullptr;active_live_visuals=studio.components_ready?live_visuals.get():nullptr;
        // An explicit integration-only tour exercises the same destination
        // path as F9, including both retained probe banks and the return trip.
        if(qa_demo_tour&&frame%240==0&&!destination_loading)next_demo_stop();
        // Capture-only integration follows the ordinary Tab event path while
        // tools stay hidden: open, close, then reopen the same retained image.
        if(qa_map_cycle&&(frame==60||frame==120||frame==180)) {
            SDL_Event key={};key.type=SDL_KEYDOWN;key.key.windowID=SDL_GetWindowID(window);
            key.key.state=SDL_PRESSED;key.key.keysym.sym=SDLK_TAB;
            if(SDL_PushEvent(&key)==1)atlas_map_qa_events++;
        }
        if(qa_training&&frame==1){SDL_RaiseWindow(window);SDL_SetWindowInputFocus(window);SDL_PumpEvents();relative_mouse_compatibility.warm(window);}
        if(qa_training&&!destination_loading){
            auto key=[&](SDL_Keycode code){SDL_Event e={};e.type=SDL_KEYDOWN;e.key.windowID=SDL_GetWindowID(window);e.key.state=SDL_PRESSED;e.key.keysym.sym=code;e.key.keysym.unused=0x56414c54u;if(SDL_PushEvent(&e)==1)qa_training_events++;};
            auto button=[&](Uint8 b,bool down){SDL_Event e={};e.type=down?SDL_MOUSEBUTTONDOWN:SDL_MOUSEBUTTONUP;e.button.windowID=SDL_GetWindowID(window);e.button.button=b;e.button.which=0x56414c54u;e.button.state=down?SDL_PRESSED:SDL_RELEASED;if(SDL_PushEvent(&e)==1)qa_training_events++;};
            if(!force_capture_first&&(frame==360||frame==540))key(SDLK_v);
            auto direction=[&](TrainingDirection d){key(d==TrainingDirection::Left?SDLK_LEFT:d==TrainingDirection::Right?SDLK_RIGHT:d==TrainingDirection::High?SDLK_UP:d==TrainingDirection::Low?SDLK_DOWN:SDLK_t);};
            if(frame==180)key(SDLK_e);
            if(qa_training_controls){
                if(frame==420||frame==480)key(SDLK_F10);
                if(frame==700||frame==900)key(SDLK_ESCAPE);
                if(frame==740)key(SDLK_e);
            }
            if(fight.state==TrainingState::Fighting&&!show_graphics&&!show_map){
                double ox,oz;world_atlas_village_origin(&ox,&oz);const double dx=ox+fight.enemy_x-cam.x,dz=oz+fight.enemy_z-cam.z,distance=std::hypot(dx,dz);if(distance>2.18){const double step=std::min(.035,distance-2.18);move_camera(cam,scenery,cam.x+dx/distance*step,cam.z+dz/distance*step);}
                if(fight.enemy_phase==TrainingEnemyPhase::Windup){
                    direction(fight.enemy_direction);
                    const bool defend=fight.enemy_duration-fight.enemy_time<.17;
                    if(defend&&!fight.guard)button(SDL_BUTTON_RIGHT,true);
                    if(!defend&&fight.guard)button(SDL_BUTTON_RIGHT,false);
                }else{
                    if(fight.guard)button(SDL_BUTTON_RIGHT,false);
                    if(fight.player_phase==TrainingPlayerPhase::Ready&&fight.stamina>=23){
                        direction(TrainingDirection((unsigned(fight.enemy_guard)+1)%5));button(SDL_BUTTON_LEFT,true);button(SDL_BUTTON_LEFT,false);
                    }
                }
            }
        }
        walking_controls_workflow(frame);
        minimap_workflow(dt);
        grass_interaction_workflow(dt);
        town_workflow();
        controller_workflow();
        preview_workflow();
        milestone_workflow();
        playtest_input_workflow(frame);
        playtest_combat_workflow(frame);
        sword_stick_workflow(frame);
        if(!severing_qa)arena_workflow(frame);
        severing_workflow(frame);
        grandstone_obstacle_workflow(frame);
        if(!road_traffic_qa&&!npc_intro_qa&&!first_journey_qa_handoff())opening_prologue_workflow(frame);
        npc_intro_qa_step(frame);
        road_traffic_qa_step(frame);
        first_journey_workflow(frame);
        bool pad_attack_requested=false,pad_thrust_requested=false;
        SDL_Event event;
        equipment_qa_step(frame);
        duel_qa_step(frame);
        joust_qa_step(frame);
        ultra_ui_step(frame);
        while (SDL_PollEvent(&event)) {
            if(ultra_ui_qa){
                if((event.type==SDL_KEYDOWN||event.type==SDL_KEYUP)&&event.key.keysym.unused!=0x56414c54u)continue;
                if((event.type==SDL_MOUSEBUTTONDOWN||event.type==SDL_MOUSEBUTTONUP)&&event.button.which!=0x56414c54u)continue;
                if(event.type==SDL_MOUSEMOTION&&event.motion.which!=0x56414c54u)continue;
                if(event.type==SDL_CONTROLLERAXISMOTION||event.type==SDL_CONTROLLERBUTTONDOWN||event.type==SDL_CONTROLLERBUTTONUP)continue;
            }
            if(joust_qa){
                if((event.type==SDL_KEYDOWN||event.type==SDL_KEYUP)&&event.key.keysym.unused!=0x56414c54u)continue;
                if((event.type==SDL_MOUSEBUTTONDOWN||event.type==SDL_MOUSEBUTTONUP)&&event.button.which!=0x56414c54u)continue;
                if(event.type==SDL_MOUSEMOTION&&event.motion.which!=0x56414c54u)continue;
                if((event.type==SDL_CONTROLLERAXISMOTION||event.type==SDL_CONTROLLERBUTTONDOWN||event.type==SDL_CONTROLLERBUTTONUP)&&event.cbutton.which!=preview_pad.id())continue;
            }
            // Integration replays only its tagged input events. Physical
            // desktop input must not trigger atlas travel or edit controls in
            // a deterministic QA run; normal play keeps ordinary SDL input.
            if(qa_training||qa_live||qa_preview||playtest_input_qa||playtest_combat_qa||grandstone_obstacle_qa||opening_qa||npc_intro_qa||road_traffic_qa||qa_stick_combat||arena_qa||equipment_qa){
                if((event.type==SDL_KEYDOWN||event.type==SDL_KEYUP)&&event.key.keysym.unused!=0x56414c54u&&!((opening_qa||npc_intro_qa||road_traffic_qa||qa_stick_combat||arena_qa||equipment_qa)&&event.key.keysym.unused==0x504144u))continue;
                if((event.type==SDL_MOUSEBUTTONDOWN||event.type==SDL_MOUSEBUTTONUP)&&event.button.which!=0x56414c54u)continue;
                if((event.type==SDL_MOUSEMOTION&&!((arena_qa||inventory_journey_qa)&&event.motion.which==0x56414c54u))||event.type==SDL_MOUSEWHEEL||event.type==SDL_TEXTINPUT)continue;
                if(grandstone_obstacle_qa&&(event.type==SDL_CONTROLLERAXISMOTION||event.type==SDL_CONTROLLERBUTTONDOWN||event.type==SDL_CONTROLLERBUTTONUP))continue;
            }
            if((opening_qa||npc_intro_qa||road_traffic_qa||qa_stick_combat||arena_qa||equipment_qa)&&(event.type==SDL_CONTROLLERAXISMOTION||event.type==SDL_CONTROLLERBUTTONDOWN||event.type==SDL_CONTROLLERBUTTONUP)&&event.cbutton.which!=preview_pad.id())continue;
            const bool pad_command=event.type==SDL_KEYDOWN&&event.key.keysym.unused==0x504144u;
            // Translated gameplay actions are one-shot engine commands, not
            // held keyboard keys. Feeding them to ImGui without a key-up could
            // leave a combat direction repeating inside a later pause menu.
            if((qa_water_refine&&(!qa_landscape||qa_refine_passed))||qa_editor_npc||qa_item_wind||qa_editor_zoom||qa_editor_reliability||qa_editor_long_session||qa_structure_splines||(qa_landscape_refine&&qa_landscape_passed)){
                if((event.type==SDL_KEYDOWN||event.type==SDL_KEYUP)&&event.key.keysym.unused!=0x56414c54u)continue;
                if((event.type==SDL_MOUSEBUTTONDOWN||event.type==SDL_MOUSEBUTTONUP)&&event.button.which!=0x56414c54u)continue;
                if(event.type==SDL_MOUSEWHEEL&&event.wheel.which!=0x56414c54u)continue;
            }
            if(!pad_command)editor_ui_process_event(&ui, &event);
            if(joust_event(event))continue;
            if(cutscene_event(event))continue;
            if(world_controller_enabled){
                if(!pad_command&&preview_pad.event(event)){
                    if(preview_screen==1)preview_screen=2;
                    preview_release_mouse();preview_notice="Controller disconnected. Reconnect or use keyboard and mouse.";preview_notice_time=8;
                }
                if(event.type==SDL_CONTROLLERDEVICEADDED||event.type==SDL_CONTROLLERDEVICEREMOVED)preview_sync_controller_ui();
                if(arena_enabled&&event.type==SDL_CONTROLLERBUTTONUP&&event.cbutton.which==preview_pad.id()&&event.cbutton.button==SDL_CONTROLLER_BUTTON_RIGHTSHOULDER){arena_focus_release(false);continue;}
                if(event.type==SDL_CONTROLLERBUTTONDOWN&&event.cbutton.which==preview_pad.id()&&preview_pad.focused){
                    if(battlefield_frame.active&&battlefield_frame.exclusive_input&&preview_screen==1){
                        preview_pad.using_pad=true;
                        if(!show_graphics&&!destination_loading){
                            using B=world_battlefield_prologue::Stage;
                            if(battlefield_frame.stage==B::Duck&&event.cbutton.button==SDL_CONTROLLER_BUTTON_X)battlefield_duck_pressed=true;
                            if(battlefield_frame.stage==B::Struggle&&event.cbutton.button==SDL_CONTROLLER_BUTTON_A)battlefield_struggle_pressed=true;
                            if(battlefield_frame.stage==B::Dead&&event.cbutton.button==SDL_CONTROLLER_BUTTON_A)battlefield_retry_pressed=true;
                        }
                        if(event.cbutton.button==SDL_CONTROLLER_BUTTON_START){preview_screen=2;opening_release();}
                        continue;
                    }
                    if(inventory_controller(SDL_GameControllerButton(event.cbutton.button)))continue;
                    if(opening_enabled&&opening_state.stage()==world_opening_prologue::Stage::GameMenu&&event.cbutton.button==SDL_CONTROLLER_BUTTON_START){
                        preview_screen=preview_screen==1?2:1;opening_release();continue;
                    }
                    if(!playtest_enabled&&!opening_enabled&&source_showcase&&preview_screen==1&&!studio.open&&!show_graphics&&!show_map&&!fight.engaged()&&
                       (event.cbutton.button==SDL_CONTROLLER_BUTTON_LEFTSHOULDER||event.cbutton.button==SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)&&!source_views.empty()){
                        if(event.cbutton.button==SDL_CONTROLLER_BUTTON_LEFTSHOULDER)source_view_index=(source_view_index+source_views.size()-2)%source_views.size();
                        next_demo_stop();preview_pad.using_pad=true;continue;
                    }
                    if(story_weapon_ready()&&(event.cbutton.button==SDL_CONTROLLER_BUTTON_DPAD_LEFT||event.cbutton.button==SDL_CONTROLLER_BUTTON_DPAD_RIGHT)){story_equip(world_weapon_loadout::melee_cycle(story_weapon,event.cbutton.button==SDL_CONTROLLER_BUTTON_DPAD_LEFT?-1:1));continue;}
                    if(arena_enabled&&event.cbutton.button==SDL_CONTROLLER_BUTTON_RIGHTSHOULDER){
                        if(arena_controls_ready()){
                            if(!arena_bow()&&SDL_GameControllerGetButton(preview_pad.get(),SDL_CONTROLLER_BUTTON_LEFTSHOULDER))pad_thrust_requested=true;
                            else arena_focus_press(false);
                        }
                        continue;
                    }
                    if(arena_enabled&&arena_controls_ready()&&(event.cbutton.button==SDL_CONTROLLER_BUTTON_DPAD_LEFT||event.cbutton.button==SDL_CONTROLLER_BUTTON_DPAD_RIGHT)){arena_weapon(world_weapon_loadout::cycle(arena_game.weapon(),event.cbutton.button==SDL_CONTROLLER_BUTTON_DPAD_LEFT?-1:1));continue;}
                    using A=world_controller::Action;
                    const auto action=world_controller::route(SDL_GameControllerButton(event.cbutton.button),active_controller_context());
                    SDL_Keycode key=0;
                    switch(action){
                        case A::Back:key=SDLK_ESCAPE;break;
                        case A::Interact:key=SDLK_e;break;
                        case A::Jump:case A::Dodge:key=SDLK_SPACE;break;
                        case A::Journal:key=SDLK_j;break;
                        case A::Camera:key=SDLK_v;break;
                        case A::Sheathe:key=SDLK_z;break;
                        case A::Left:key=SDLK_LEFT;break;case A::Right:key=SDLK_RIGHT;break;
                        case A::High:key=SDLK_UP;break;case A::Low:key=SDLK_DOWN;break;
                        case A::Attack:pad_attack_requested=true;break;
                        case A::Thrust:pad_thrust_requested=true;break;
                        case A::PreviousView:case A::NextView:{
                            const int direction=action==A::NextView?1:2;
                            preview_review(world_milestone::View((int(milestone_review.view)-1+direction)%3+1));break;
                        }
                        default:break;
                    }
                    // UI owns confirm/navigation in menus, dialogue and retry.
                    // Gameplay commands re-use the ordinary keyboard route once.
                    if(opening_enabled&&event.cbutton.button==SDL_CONTROLLER_BUTTON_A)
                        key=world_controller::ui_owns(preview_controller_context())?SDLK_RETURN:0;
                    if(key){SDL_Event input={};input.type=SDL_KEYDOWN;input.key.windowID=SDL_GetWindowID(window);input.key.state=SDL_PRESSED;input.key.keysym.sym=key;input.key.keysym.unused=0x504144u;SDL_PushEvent(&input);}
                }
                if(pad_command)preview_pad.using_pad=true;
            }
            if (event.type == SDL_QUIT||(event.type==SDL_WINDOWEVENT&&event.window.event==SDL_WINDOWEVENT_CLOSE)){
                if(studio.designer_should_confirm_close()){studio.designer_close_requested=true;studio.open=true;studio.landscape_cancel();studio.cancel_designer_drag();studio.designer_asset_cancel();look_active=false;SDL_SetRelativeMouseMode(SDL_FALSE);}
                else running=false;
                continue;
            }
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                if(opening_enabled){opening_release();opening_confirm=opening_interact=opening_cancel=opening_enter=false;}
                look_active = false;fight.set_guard(false);fight.clear_buffer();if(arena_enabled){arena_modal_neutral();arena_game.cancel_draw();arena_mouse_draw=arena_mouse_aim=false;arena_bow_armed=false;}
                if(presentation&&preview_screen==1&&!launch->frames)preview_screen=2;
                studio.landscape_cancel();studio.cancel_designer_drag();
                SDL_SetRelativeMouseMode(SDL_FALSE);
            }
            if(event.type==SDL_WINDOWEVENT&&event.window.event==SDL_WINDOWEVENT_FOCUS_GAINED)relative_mouse_compatibility.warm(window);
            if(event.type==SDL_WINDOWEVENT&&event.window.event==SDL_WINDOWEVENT_FOCUS_GAINED&&!studio.open&&fight.engaged()&&(!presentation||!preview_pad.using_pad)&&!show_map&&!show_graphics&&!studio.production_freeze()&&!studio.cinematic_preview){look_active=true;SDL_SetRelativeMouseMode(SDL_TRUE);}
            if (event.type == SDL_WINDOWEVENT &&(event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED||event.window.event==SDL_WINDOWEVENT_DISPLAY_CHANGED)) {
                int w, h;
                SDL_GL_GetDrawableSize(window, &w, &h);
                if (w > 0 && h > 0) {
                    studio.landscape_cancel();studio.cancel_designer_drag();
                    display_w=w;display_h=h;display_resize=true;
                    SDL_GetWindowSize(window,&logical_w,&logical_h);refresh_designer_view();
                    quality_valid_after=frame+60;
                    if(event.window.event==SDL_WINDOWEVENT_DISPLAY_CHANGED){
                        graphics_monitor_refresh=graphics_refresh_hz(window);
                        quality.reset({graphics.auto_quality,graphics.render_scale,graphics.min_render_scale,
                            float(world_graphics_effective_target_fps(graphics,graphics_refresh_hz(window)))},++quality_epoch);
                    }
                }
            }
            if(inventory_enabled&&(inventory_open||!inventory_controls_ready)&&(event.type==SDL_MOUSEBUTTONDOWN||event.type==SDL_MOUSEBUTTONUP||event.type==SDL_MOUSEMOTION||event.type==SDL_MOUSEWHEEL))continue;
            if(opening_enabled&&(event.type==SDL_KEYDOWN||event.type==SDL_KEYUP)&&!pad_command&&!event.key.repeat){
                const bool down=event.type==SDL_KEYDOWN;
                const auto k=event.key.keysym.sym;
                if(down&&battlefield_frame.active&&preview_screen==1&&preview_pad.focused&&!show_graphics&&!destination_loading){
                    using B=world_battlefield_prologue::Stage;
                    if(battlefield_frame.stage==B::Duck&&k==SDLK_q)battlefield_duck_pressed=true;
                    if(battlefield_frame.stage==B::Struggle&&k==SDLK_f)battlefield_struggle_pressed=true;
                    if(battlefield_frame.stage==B::Dead&&(k==SDLK_RETURN||k==SDLK_KP_ENTER||k==SDLK_r))battlefield_retry_pressed=true;
                }
                if(k==SDLK_RETURN||k==SDLK_KP_ENTER||k==SDLK_SPACE)opening_confirm=down;
                if(k==SDLK_RETURN||k==SDLK_KP_ENTER)opening_enter=down;
                if(k==SDLK_e)opening_interact=down;
                if(k==SDLK_r&&!down)first_journey_retry_down=false;
                if(k==SDLK_ESCAPE)opening_cancel=down;
            }
            if(opening_enabled&&event.type==SDL_MOUSEBUTTONDOWN&&opening_controls_enabled&&!show_graphics&&!destination_loading&&!launch->frames){look_active=true;preview_pad.using_pad=false;SDL_SetRelativeMouseMode(SDL_TRUE);}
            if (event.type == SDL_KEYDOWN && !launch->benchmark &&
                (!launch->frames || qa_training || qa_live || qa_preview || qa_milestone || qa_controller || qa_town || qa_grass || qa_minimap || qa_walking_controls || opening_qa || npc_intro_qa || road_traffic_qa || equipment_qa || playtest_input_qa || playtest_combat_qa || qa_stick_combat || arena_qa || qa_landscape || qa_structure || qa_editor_npc || qa_item_wind || qa_editor_zoom || qa_editor_reliability || qa_editor_long_session || qa_structure_splines || qa_landscape_refine || qa_water_refine || (launch->frames&&studio.path.rfind("/tmp/",0)==0&&(std::getenv("VALWOOD_QA_DESIGNER_WORKSPACE")||std::getenv("VALWOOD_QA_EDITOR_COPY_PASTE")||std::getenv("VALWOOD_QA_EDITOR_TERRAIN_SNAP")||std::getenv("VALWOOD_QA_EDITOR_VFX"))) || (qa_map_cycle && event.key.keysym.sym==SDLK_TAB)) &&
                !event.key.repeat && !ImGui::GetIO().WantTextInput) {
                int k = event.key.keysym.sym;
                if(inventory_key(SDL_Keycode(k)))continue;
                if(story_weapon_ready()&&k>=SDLK_1&&k<=SDLK_5){if(k!=SDLK_2)story_equip(world_weapon_loadout::Weapon(k-SDLK_1));else playtest_tell("Bow practice is available in the combat arena.");continue;}
                if(k==SDLK_F7&&studio.designer_restore_selection_after_play&&studio.production_playing()){
                    if(studio.editor_session->request_stop())studio.production_stop(world_editor::StopChoice::DiscardPlayChanges);
                    fight.leave();look_active=false;SDL_SetRelativeMouseMode(SDL_FALSE);continue;
                }
                if(preview_key(SDL_Keycode(k)))continue;
                if(arena_enabled&&!arena_controls_ready()&&k!=SDLK_F10)continue;
                if(k==SDLK_F11&&!ImGui::IsAnyItemActive()){studio.landscape_cancel();studio.cancel_designer_drag();studio.designer_asset_cancel();studio.open=!studio.open;show_tools=false;show_map=show_graphics=false;fight.leave();look_active=false;SDL_SetRelativeMouseMode(SDL_FALSE);sync_designer_camera();refresh_designer_view();continue;}
                if(studio.open){
                    if(k==SDLK_BACKSPACE&&!studio.landscape_river_points.empty()){studio.landscape_river_points.pop_back();continue;}
                    // Authoring owns these keys before gameplay. A focused text
                    // or numeric widget keeps its own shortcuts and keystrokes.
                    if(k==SDLK_ESCAPE&&(studio.drag.active||studio.designer_asset_placing()||(!ImGui::IsAnyItemActive()&&!studio.designer_blocks_viewport()))){studio.landscape_cancel();studio.cancel_designer_drag();studio.designer_asset_cancel();studio.vfx_placing=false;show_graphics=false;look_active=false;SDL_SetRelativeMouseMode(SDL_FALSE);}
                    using Key=world_designer_viewport::Key;Key key=Key::None;
                    switch(k){case SDLK_q:key=Key::Q;break;case SDLK_w:key=Key::W;break;case SDLK_e:key=Key::E;break;case SDLK_r:key=Key::R;break;case SDLK_f:key=Key::F;break;case SDLK_p:key=Key::P;break;case SDLK_c:key=Key::C;break;case SDLK_v:key=Key::V;break;case SDLK_d:key=Key::D;break;case SDLK_s:key=Key::S;break;case SDLK_z:key=Key::Z;break;case SDLK_y:key=Key::Y;break;case SDLK_DELETE:key=Key::Delete;break;case SDLK_BACKSPACE:key=Key::Backspace;break;case SDLK_END:key=Key::End;break;default:break;}
                    const auto modifiers=event.key.keysym.mod;world_designer_viewport::ShortcutContext context;
                    context.text_input=ImGui::GetIO().WantTextInput;context.active_widget=ImGui::IsAnyItemActive()||studio.drag.active;context.blocked=studio.designer_blocks_viewport()||studio.production_freeze()||studio.cinematic_preview;context.navigating=look_active;
                    context.ctrl=(modifiers&KMOD_CTRL)!=0;context.shift=(modifiers&KMOD_SHIFT)!=0;context.alt=(modifiers&KMOD_ALT)!=0;context.super=(modifiers&KMOD_GUI)!=0;
                    if(!context.text_input&&!context.active_widget&&!context.blocked&&!context.navigating&&!context.alt&&!context.super){
                        int axis=-1,sign=0;
                        if(k==SDLK_LEFT){axis=0;sign=-1;}else if(k==SDLK_RIGHT){axis=0;sign=1;}
                        else if(k==SDLK_UP){axis=2;sign=-1;}else if(k==SDLK_DOWN){axis=2;sign=1;}
                        else if(k==SDLK_PAGEUP){axis=1;sign=1;}else if(k==SDLK_PAGEDOWN){axis=1;sign=-1;}
                        if(axis>=0){studio.designer_nudge(axis,sign,context.ctrl,context.shift);continue;}
                    }
                    world_designer_viewport::Command command=world_designer_viewport::Command::Select;
                    if(world_designer_viewport::shortcut(key,context,command))studio.designer_command(command);
                    continue;
                }
                if(studio.production_freeze()||studio.cinematic_preview)continue;
                if(k==SDLK_j){journey_open=!journey_open;look_active=false;SDL_SetRelativeMouseMode(SDL_FALSE);}
                if(k==SDLK_F5){save_journey();journey_open=true;}
                if(k==SDLK_F8){load_journey();journey_open=true;}
                if(k==SDLK_z&&(world_combat_motion_unarmed_ready(combat_motion)||(skeletal_hero_loaded&&skeletal_hero.supports_equipment()))&&preview_screen==1&&!show_map&&!show_graphics&&!destination_loading&&!first_journey_dialogue_active&&(!opening_enabled||opening_controls_enabled)&&(!first_journey_enabled||story_sword_unlocked())&&!cam.swimming){
                    if(fight.player_phase==TrainingPlayerPhase::Ready&&!equipment_change.active()){equipment_action_gate.reset();weapon_sheathed=!weapon_sheathed;if(weapon_sheathed)++weapon_stows;else ++weapon_draws;fight.set_guard(false);fight.clear_buffer();sword_stick.reset();if(arena_enabled){arena_game.cancel_draw();arena_mouse_draw=arena_mouse_aim=false;arena_bow_armed=false;}}
                    continue;
                }
                if(k==SDLK_v){third_person=!third_person;camera_toggles++;temporal_cut=true;}
                if(world_controller_enabled&&k==SDLK_SPACE&&!fight.engaged()&&!preview_blocked()&&!milestone_review.portrait()&&!destination_loading&&cam.grounded){cam.vy=4.7f;cam.grounded=false;}
                if(k==SDLK_e&&!studio.open&&!cam.fly&&!show_map&&!show_graphics&&!destination_loading&&studio.components_interact({cam.x,cam.y-1.75,cam.z})){look_active=false;SDL_SetRelativeMouseMode(SDL_FALSE);continue;}
                if(k==SDLK_e&&!studio.open&&!cam.fly&&!show_map&&!show_graphics&&!destination_loading&&training){
                    double ox,oz;world_atlas_village_origin(&ox,&oz);
                    const bool retry=!fight.engaged()&&fight.state!=TrainingState::Idle&&std::hypot(cam.x-ox-fight.enemy_x,cam.z-oz-fight.enemy_z)<8.;
                    if(retry&&studio.components_runtime&&studio.components_runtime->enqueue({world_gameplay::EventKind::Interact,world_runtime_demo::instructor_id})){look_active=false;SDL_SetRelativeMouseMode(SDL_FALSE);continue;}
                    if(!fight.engaged())fight.difficulty=preview_settings.difficulty;
                    if(fight.start(retry?fight.instructor_x:cam.x-ox,cam.y,retry?fight.instructor_z:cam.z-oz)){
                        fight_starts++;fight_begin_frame=frame;fight_end_frame=0;
                        cam.x=ox+fight.start_x;cam.z=oz+fight.start_z;cam.y=25.75;cam.vy=0;cam.grounded=true;cam.fly=false;
                        cam.yaw=1.57079633f;cam.pitch=-.12f;temporal_cut=true;look_active=true;fight_mouse_x=fight_mouse_y=0;
                        fight_capture_focused=(SDL_GetWindowFlags(window)&SDL_WINDOW_INPUT_FOCUS)!=0;
                        const Uint64 capture_begin=SDL_GetPerformanceCounter();SDL_SetRelativeMouseMode(SDL_TRUE);
                        const double capture_ms=1000.*double(SDL_GetPerformanceCounter()-capture_begin)/SDL_GetPerformanceFrequency();
                        fight_capture_max_ms=std::max(fight_capture_max_ms,capture_ms);
                        std::printf("[Training] Mouse capture %.3fms, focused %s\n",capture_ms,fight_capture_focused?"yes":"no");
                    }
                }
                if(fight.engaged()&&!show_map&&!show_graphics){
                    if(k==SDLK_LEFT)fight.choose(TrainingDirection::Left);
                    if(k==SDLK_RIGHT)fight.choose(TrainingDirection::Right);
                    if(k==SDLK_UP)fight.choose(TrainingDirection::High);
                    if(k==SDLK_DOWN)fight.choose(TrainingDirection::Low);
                    if(k==SDLK_t)fight.choose(TrainingDirection::Thrust);
                    if(k==SDLK_SPACE&&fight.dodge())training_sounds.play(3,30);
                }
                if (k == SDLK_TAB) {
                    show_map = !show_map;
                    if(show_map){look_active=false;SDL_SetRelativeMouseMode(SDL_FALSE);}
                }
                if(k==SDLK_F10){show_graphics=!show_graphics;graphics_draft=graphics;look_active=false;SDL_SetRelativeMouseMode(SDL_FALSE);}
                if(k==SDLK_F9){
                    fight.leave();look_active=false;SDL_SetRelativeMouseMode(SDL_FALSE);
                    next_demo_stop();
                }
                if (k == SDLK_h) {show_tools=!show_tools;if(!show_tools)show_map=false;}
                if (k == SDLK_ESCAPE) {
                    fight.leave();
                    show_graphics=false;
                    show_map = false;
                    look_active = false;
                    SDL_SetRelativeMouseMode(SDL_FALSE);
                }
                if (k == SDLK_f) {
                    fight.leave();
                    cam.fly = !cam.fly;
                    cam.y = std::max(world_atlas_height(cam.x, cam.z), 0.f) + (cam.fly ? 150. : 1.75);
                    cam.pitch = cam.fly ? -.35f : -.04f;
                    temporal_cut=true;
                }
                if (k >= SDLK_1 &&k <= SDLK_9) {
                    fight.leave();
                    current = (k - SDLK_1) % int(landmark_count);
                    teleport(cam, landmarks[current]);
                    current_name=landmarks[current].name;destination_loading=true;
                    temporal_cut=true;
                    benchmark_x = cam.x;
                    benchmark_z = cam.z;
                }
            }
            if(studio.open){
                const bool eligible=!studio.drag.active&&(!launch->frames||qa_structure||qa_editor_zoom||qa_editor_reliability||qa_editor_long_session||qa_structure_splines||qa_landscape_refine)&&!launch->benchmark&&!studio.production_freeze()&&!studio.cinematic_preview&&!studio.designer_blocks_viewport();
                int wheel_x=0,wheel_y=0;Uint32 wheel_buttons=0;
                if(event.type==SDL_MOUSEWHEEL){
                    wheel_buttons=SDL_GetMouseState(&wheel_x,&wheel_y);
                    const int raw_x=wheel_x,raw_y=wheel_y;
                    // Match the UI backend's focused-window pointer snapshot.
                    // X11 can retain a stale local position after releasing a
                    // relative-mode grab, even after the desktop cursor moves.
                    const char *driver=SDL_GetCurrentVideoDriver();
                    if(driver&&!std::strcmp(driver,"x11")&&!SDL_GetRelativeMouseMode()){
                        int global_x=0,global_y=0,window_x=0,window_y=0;
                        wheel_buttons=SDL_GetGlobalMouseState(&global_x,&global_y);SDL_GetWindowPosition(window,&window_x,&window_y);
                        wheel_x=global_x-window_x;wheel_y=global_y-window_y;
                    }
                    if(qa_structure){auto &p=structure_wheel_probe;++p.events;p.buttons=wheel_buttons;p.raw_x=raw_x;p.raw_y=raw_y;p.x=wheel_x;p.y=wheel_y;
                        p.eligible=eligible;p.capture=editor_ui_wants_mouse(&ui);p.text=ImGui::GetIO().WantTextInput;p.active=ImGui::IsAnyItemActive();p.focus=(SDL_GetWindowFlags(window)&SDL_WINDOW_INPUT_FOCUS)!=0;p.app_focus=!ImGui::GetIO().AppFocusLost;p.inside=designer_view.contains(float(wheel_x),float(wheel_y));}
                }
                if(event.type==SDL_MOUSEBUTTONUP&&event.button.button==SDL_BUTTON_RIGHT){look_active=false;SDL_SetRelativeMouseMode(SDL_FALSE);}
                else if(event.type==SDL_MOUSEBUTTONDOWN&&event.button.button==SDL_BUTTON_RIGHT&&eligible&&designer_view.contains(float(event.button.x),float(event.button.y))&&!editor_ui_wants_mouse(&ui)&&!ImGui::IsAnyItemActive()){
                    look_active=true;SDL_SetRelativeMouseMode(SDL_TRUE);
                }else if(event.type==SDL_MOUSEMOTION&&look_active&&eligible){
                    cam.yaw+=event.motion.xrel*.0022f;cam.pitch=std::clamp(cam.pitch-event.motion.yrel*.0022f,-1.48f,1.48f);
                }else if(event.type==SDL_MOUSEWHEEL&&eligible&&!editor_ui_wants_mouse(&ui)&&
                         event.wheel.windowID==SDL_GetWindowID(window)&&(SDL_GetWindowFlags(window)&SDL_WINDOW_INPUT_FOCUS)&&
                         !ImGui::GetIO().AppFocusLost&&!ImGui::GetIO().WantTextInput&&!ImGui::IsAnyItemActive()&&
                         !studio.landscape_painting&&!studio.designer_asset_placing()&&!studio.vfx_placing){
                    if(designer_view.contains(float(wheel_x),float(wheel_y))&&!(wheel_buttons&(SDL_BUTTON_LMASK|SDL_BUTTON_MMASK))){
                        double scroll=event.wheel.y;
#if SDL_VERSION_ATLEAST(2,0,18)
                        if(event.wheel.preciseY!=0)scroll=event.wheel.preciseY;
#endif
                        if(event.wheel.direction==SDL_MOUSEWHEEL_FLIPPED)scroll=-scroll;
                        if(std::isfinite(scroll)&&scroll!=0){
                            scroll=std::clamp(scroll,-8.,8.);
                            if(look_active){studio.fly_speed=std::clamp(studio.fly_speed*float(std::exp(scroll*.15)),1.f,1000.f);++editor_speed_events;}
                            else if(editor_camera_dolly(cam,scroll,opening_interior)){temporal_cut=true;++editor_dolly_events;}
                        }
                    }
                }
                continue;
            }
            if((preview_blocked()||studio.production_freeze()||studio.cinematic_preview)&&(event.type==SDL_MOUSEBUTTONDOWN||event.type==SDL_MOUSEBUTTONUP||event.type==SDL_MOUSEMOTION))continue;
            if(arena_enabled&&(event.type==SDL_MOUSEBUTTONDOWN||event.type==SDL_MOUSEBUTTONUP)&&event.button.button==SDL_BUTTON_MIDDLE){
                if(event.type==SDL_MOUSEBUTTONUP)arena_focus_release(true);
                else if(arena_controls_ready()){
                    if(!arena_bow()&&SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_Q]&&fight.engaged()){
                        if(!equipment_request_draw()){fight.choose(TrainingDirection::Thrust);skeletal_timed_attack();}
                    }else arena_focus_press(true);
                }
                continue;
            }
            if(arena_enabled&&arena_controls_ready()&&arena_bow()&&(!launch->frames||arena_qa||equipment_qa)&&preview_pad.focused&&!preview_blocked()&&!show_map&&!show_graphics&&!destination_loading){
                if(event.type==SDL_MOUSEBUTTONDOWN||event.type==SDL_MOUSEBUTTONUP){const bool down=event.type==SDL_MOUSEBUTTONDOWN;if(event.button.button==SDL_BUTTON_LEFT)arena_mouse_draw=down;if(event.button.button==SDL_BUTTON_RIGHT){arena_mouse_aim=down;look_active=down||arena_mouse_draw;SDL_SetRelativeMouseMode(look_active?SDL_TRUE:SDL_FALSE);}continue;}
                if(event.type==SDL_MOUSEMOTION&&(look_active||arena_mouse_draw||fight.engaged())){cam.yaw+=event.motion.xrel*.0022f*preview_settings.sensitivity;cam.pitch=std::clamp(cam.pitch-event.motion.yrel*.0022f*preview_settings.sensitivity*(preview_settings.invert_y?-1.f:1.f),-1.1f,1.1f);continue;}
            }
            if((!arena_enabled||arena_controls_ready())&&fight.engaged()&&(!launch->frames||qa_training||qa_preview||qa_controller||playtest_combat_qa||qa_stick_combat||arena_qa||equipment_qa)&&!launch->benchmark&&!show_map&&!show_graphics&&!destination_loading){
                if(event.type==SDL_MOUSEBUTTONDOWN){
                    if(event.button.button==SDL_BUTTON_LEFT){if(!equipment_request_draw())skeletal_timed_attack();}
                    if(event.button.button==SDL_BUTTON_MIDDLE){if(!equipment_request_draw()){fight.choose(TrainingDirection::Thrust);skeletal_timed_attack();}}
                    if(event.button.button==SDL_BUTTON_RIGHT&&equipment_actions_ready())fight.set_guard(true);
                    continue;
                }
                if(event.type==SDL_MOUSEBUTTONUP){if(event.button.button==SDL_BUTTON_RIGHT)fight.set_guard(false);continue;}
                if(event.type==SDL_MOUSEMOTION&&(arena_look_held()||weapon_sheathed)){
                    cam.yaw+=event.motion.xrel*.0022f*preview_settings.sensitivity;
                    cam.pitch=std::clamp(cam.pitch-event.motion.yrel*.0022f*preview_settings.sensitivity*(preview_settings.invert_y?-1.f:1.f),-1.1f,1.1f);
                    fight.set_guard(false);
                    if(!(arena_enabled&&fight.pending_attack&&fight.pending_direction==TrainingDirection::Thrust))fight.clear_buffer();
                    continue;
                }
                if(event.type==SDL_MOUSEMOTION){
                    fight_mouse_x+=event.motion.xrel;fight_mouse_y+=event.motion.yrel;
                    if(std::max(std::abs(fight_mouse_x),std::abs(fight_mouse_y))>=12){
                        fight.choose(std::abs(fight_mouse_x)>std::abs(fight_mouse_y)?(fight_mouse_x<0?TrainingDirection::Left:TrainingDirection::Right):(fight_mouse_y<0?TrainingDirection::High:TrainingDirection::Low));
                        fight_mouse_x=fight_mouse_y=0;
                    }
                    continue;
                }
            }
            if (!opening_enabled&&!launch->frames && !launch->benchmark && event.type == SDL_MOUSEBUTTONDOWN &&event.button.button == SDL_BUTTON_RIGHT && !editor_ui_wants_mouse(&ui)) {
                look_active = true;
                SDL_SetRelativeMouseMode(SDL_TRUE);
            }
            if (!opening_enabled&&event.type == SDL_MOUSEBUTTONUP &&event.button.button == SDL_BUTTON_RIGHT) {
                look_active = false;
                SDL_SetRelativeMouseMode(SDL_FALSE);
            }
            if(milestone_review.portrait()&&event.type==SDL_MOUSEWHEEL){milestone_review.zoom(float(event.wheel.y));continue;}
            if (event.type == SDL_MOUSEMOTION &&look_active&&(!opening_enabled||opening_controls_enabled)) {
                if(milestone_review.portrait()){milestone_review.drag(float(event.motion.xrel),float(event.motion.yrel));continue;}
                cam.yaw += event.motion.xrel * .0022f*(presentation?preview_settings.sensitivity:1.f);
                cam.pitch = std::clamp(cam.pitch - event.motion.yrel * .0022f*(presentation?preview_settings.sensitivity*(preview_settings.invert_y?-1.f:1.f):1.f), -1.48f, 1.48f);
            }
        }

        if(!running)break;
        inventory_tick();
        story_sync_appearance(frame);
        // The bounded, marker-validated offscreen rehearsal owns its held
        // inputs. An absent real window focus must not release that guard.
        if(!combat_feedback_qa&&!arena_roster_qa&&!blade_clash_qa)arena_refresh_input();
        sync_designer_camera();refresh_designer_view();
        if(studio.open&&studio.designer_frame_pending){
            studio.designer_frame_pending=false;
            const bool npc_frame=studio.npc_frame_pending;studio.npc_frame_pending=false;
            if(studio.designer_frame_reveal){cam.pitch=npc_frame?-.18f:std::clamp(cam.pitch,-.85f,-.45f);cam.yaw=studio.designer_frame_heading;studio.designer_frame_reveal=false;}
            const auto center=studio.designer_frame_center;const double radius=std::clamp(studio.designer_frame_radius,.25,100000.);
            if(std::isfinite(center.x)&&std::isfinite(center.y)&&std::isfinite(center.z)&&std::isfinite(radius)){
                cam.aspect=world_designer_viewport::aspect(designer_view);const double half=std::min(double(cam.fov*.5f),std::atan(std::tan(double(cam.fov*.5f))*cam.aspect));
                const double distance=radius/std::max(std::sin(half),.01)*1.18;const auto forward=cam.forward();
                cam.x=std::clamp(center.x-forward.x*distance,-world_atlas_width()*.5+1,world_atlas_width()*.5-1);
                cam.z=std::clamp(center.z-forward.z*distance,-world_atlas_depth()*.5+1,world_atlas_depth()*.5-1);cam.y=std::clamp(center.y-forward.y*distance,-12000.,12000.);
                if(npc_frame){
                    // Keep the NPC view below typical tree canopies. Hills at
                    // the new eye position still require full standing clearance;
                    // after that correction, aim back at the actual path center.
                    const double ground=world_atlas_walk_height(cam.x,cam.z);
                    if(std::isfinite(ground))cam.y=std::max(cam.y,ground+1.8);
                    const double dx=center.x-cam.x,dz=center.z-cam.z;
                    cam.yaw=float(std::atan2(dx,-dz));cam.pitch=std::clamp(float(std::atan2(center.y-cam.y,std::hypot(dx,dz))),-1.48f,1.48f);
                }
                cam.fly=true;cam.grounded=false;cam.vy=0;temporal_cut=true;
            }
        }
        if(!authored_cutscene_locked&&studio.open&&!studio.drag.active&&!launch->frames&&!launch->benchmark&&look_active&&!studio.production_freeze()&&!studio.cinematic_preview&&!studio.designer_blocks_viewport()&&!ImGui::GetIO().WantTextInput&&!ImGui::IsAnyItemActive()){
            const auto *keys=SDL_GetKeyboardState(nullptr);
            const bool indoor=editor_camera_near_interior(cam,opening_interior);
            const float base_speed=indoor?std::clamp(studio.fly_speed*.15f,1.f,12.f):studio.fly_speed;
            const float speed=base_speed*((keys[SDL_SCANCODE_LSHIFT]||keys[SDL_SCANCODE_RSHIFT])?(indoor?2.f:4.f):1.f)*
                              ((keys[SDL_SCANCODE_LCTRL]||keys[SDL_SCANCODE_RCTRL])?.25f:1.f);
            const auto forward=cam.forward(),right=cam.right();const float f=float(keys[SDL_SCANCODE_W])-float(keys[SDL_SCANCODE_S]),s=float(keys[SDL_SCANCODE_D])-float(keys[SDL_SCANCODE_A]),v=float(keys[SDL_SCANCODE_E])-float(keys[SDL_SCANCODE_Q]);
            V3 delta=add(add(mul(forward,f),mul(right,s)),{0,v,0});const float length=std::sqrt(dot(delta,delta));if(length>1)delta=mul(delta,1/length);
            cam.x=std::clamp(cam.x+delta.x*speed*dt,-world_atlas_width()*.5+1,world_atlas_width()*.5-1);cam.z=std::clamp(cam.z+delta.z*speed*dt,-world_atlas_depth()*.5+1,world_atlas_depth()*.5-1);cam.y=std::clamp(cam.y+delta.y*speed*dt,-12000.,12000.);
            cam.fly=true;cam.grounded=false;cam.vy=0;
        }

        if(world_controller_enabled&&preview_pad.using_pad&&look_active){look_active=false;SDL_SetRelativeMouseMode(SDL_FALSE);}
        const auto pad_context=active_controller_context();
        const bool pad_playable=!authored_cutscene_locked&&!cutscene_return_neutral&&world_controller_enabled&&preview_pad.focused&&!studio.open&&!preview_blocked()&&!show_map&&!show_graphics&&!destination_loading&&
            !world_controller::ui_owns(pad_context);
        preview_pad.poll(preview_settings.deadzone,pad_playable&&!milestone_review.active());
        if(pad_playable&&milestone_review.portrait()){
            milestone_review.drag(preview_pad.look.x*dt*450.f,preview_pad.look.y*dt*300.f*(preview_settings.invert_y?-1.f:1.f));
            milestone_review.zoom((preview_pad.right_trigger-preview_pad.left_trigger)*dt*8.f);
        }
        cutscene_tick(dt);
        const bool player_controls_allowed=!joust_enabled&&!authored_cutscene_locked&&!cutscene_return_neutral&&inventory_controls_ready&&(!arena_enabled||arena_controls_ready())&&!milestone_review.portrait()&&!preview_blocked()&&!studio.open&&!(studio.components_runtime&&studio.components_runtime->current_line())&&!studio.production_freeze()&&!studio.cinematic_preview&&(!world_controller_enabled||preview_pad.focused||animation_polish_qa||equipment_qa||blade_clash_qa)&&(!launch->frames||qa_autowalk||qa_controller||qa_town||opening_qa||npc_intro_qa||road_traffic_qa||qa_stick_combat||arena_qa||qa_grass||qa_minimap||qa_walking_controls||animation_polish_qa||equipment_qa||blade_clash_qa) && !launch->benchmark && !ImGui::GetIO().WantCaptureKeyboard && !show_map && !show_graphics && !destination_loading &&
            terrain.view_ready(cam.x, cam.z, cam.fly);
        const Uint32 equipment_mouse=SDL_GetMouseState(nullptr,nullptr);
        const bool equipment_attack_down=(equipment_mouse&(SDL_BUTTON_LMASK|SDL_BUTTON_MMASK))||preview_pad.right_trigger>.15f||pad_attack_requested||pad_thrust_requested||
            (!arena_bow()&&preview_pad.using_pad&&std::hypot(preview_pad.look.x,preview_pad.look.y)>world_controller::SwordStick::neutral_radius);
        const bool equipment_guard_down=(equipment_mouse&SDL_BUTTON_RMASK)||preview_pad.left_trigger>.15f;
        const bool equipment_neutral_ready=!story_unarmed_body()&&!weapon_sheathed&&!equipment_change.active()&&
            (!skeletal_hero_loaded||!WorldSkeletalHeroLoadouts::supports_weapon(fight.weapon)||!skeletal_hero.supports_equipment()||skeletal_hero.equipment_combat_ready());
        if(!player_controls_allowed)equipment_action_gate.reset();
        else equipment_action_gate.update(equipment_neutral_ready,equipment_attack_down,equipment_guard_down);
        // Sample even while menus/loading own input so a held stick cannot
        // become a delayed swing when ordinary combat controls resume.
        const bool sword_pad_context=player_controls_allowed&&pad_playable&&preview_pad.get()&&fight.engaged()&&equipment_actions_ready()&&!arena_bow()&&!arena_look_held();
        const bool sword_pad_enabled=sword_pad_context&&preview_pad.using_pad;
        const bool sword_guard_held=preview_pad.left_trigger>.4f;
        // A centered connected controller can arm while keyboard/mouse owns
        // input. Its first deliberate stick movement then works immediately;
        // an inactive deflected stick remains disarmed and cannot replay later.
        const bool sword_stick_neutral=std::hypot(preview_pad.look.x,preview_pad.look.y)<=world_controller::SwordStick::neutral_radius;
        const auto sword_gesture=sword_stick.sample(preview_pad.look,sword_pad_context&&(preview_pad.using_pad||sword_stick_neutral),sword_guard_held,fight.state==TrainingState::Fighting);
        // Only ordinary supported walking carries a short velocity blend.
        // Modal ownership, teleports, jumping, swimming and flight discard it;
        // collision commits below prevent stored pressure building at a wall.
        const bool player_motion_relocated=std::hypot(cam.x-player_move_last_x,cam.z-player_move_last_z)>.001;
        if(!player_controls_allowed||!player_move_initialized||player_motion_relocated)
            player_move.reset();
        const bool player_hitstop=fight.impact_pause>0;
        WorldSkeletalHeroLoadouts::MotionRequest captured_walk_request;
        bool captured_walk_attempted=false,captured_walk_ok=true;
        const std::uint64_t player_contact_before=std::uint64_t(fight.hits)+fight.guarded+fight.parries+fight.blocks+fight.enemy_hits;
        if (player_controls_allowed) {
            const Uint8 *keys = SDL_GetKeyboardState(nullptr);
            const float pad_forward=pad_playable?-preview_pad.move.y:0;
            const float pad_side=pad_playable?preview_pad.move.x:0;
            if(pad_playable){
                const auto camera_stick=world_controller::camera_stick(preview_pad.look);
                const float px=camera_stick.x,py=camera_stick.y;
                if(fight.engaged()&&!arena_bow()){
                    if(weapon_sheathed&&!sword_guard_held&&(preview_pad.attack_pressed||pad_attack_requested||pad_thrust_requested)){
                        equipment_request_draw();pad_thrust_requested=pad_attack_requested=false;
                    }
                    // The arena keeps RB for focus; its LB+RB chord requests a
                    // stab while LB also owns free look. Resolve that intent
                    // before the free-look branch, and never through held LT.
                    if(arena_enabled&&pad_thrust_requested&&!sword_guard_held&&!weapon_sheathed){
                        fight.choose(TrainingDirection::Thrust);skeletal_timed_attack();pad_thrust_requested=false;
                    }
                    if(arena_look_held()||weapon_sheathed){
                        cam.yaw+=px*dt*2.2f*preview_settings.sensitivity;
                        cam.pitch=std::clamp(cam.pitch-py*dt*1.5f*preview_settings.sensitivity*(preview_settings.invert_y?-1.f:1.f),-1.1f,1.1f);
                        fight.set_guard(false);
                        if(!(arena_enabled&&fight.pending_attack&&fight.pending_direction==TrainingDirection::Thrust))fight.clear_buffer();
                    }else if(sword_pad_enabled){
                        if(sword_gesture.select_direction)fight.choose(sword_gesture.direction);
                        fight.set_guard(equipment_actions_ready()&&preview_pad.guard);
                        // LT wins even when buttons/RT arrive in the same SDL
                        // frame, and while a held trigger awaits neutral rearm.
                        if(!sword_guard_held){
                            if(pad_thrust_requested){fight.choose(TrainingDirection::Thrust);skeletal_timed_attack();}
                            else if(sword_gesture.attack||preview_pad.attack_pressed||pad_attack_requested)skeletal_timed_attack();
                        }
                    }
                }else{cam.yaw+=px*dt*2.2f*preview_settings.sensitivity;cam.pitch=std::clamp(cam.pitch-py*dt*1.5f*preview_settings.sensitivity*(preview_settings.invert_y?-1.f:1.f),-1.45f,1.45f);}
            }
            if(arena_enabled&&arena_target_lock.locked()&&!arena_bow()&&!arena_look_held())
                cam.yaw=arena_target_lock.steer(arena_game,{cam.x-arena_origin_x,cam.y-1.75-arena_floor,cam.z-arena_origin_z},cam.yaw,dt);
            if(fight.engaged()&&!look_active&&(!presentation||!preview_pad.using_pad)){look_active=true;SDL_SetRelativeMouseMode(SDL_TRUE);}
            // The opening launch stays active throughout the outdoor demo.
            // Restrict water controls by its current stage, not launch mode.
            const bool water_controls_allowed=!opening_enabled||
                opening_state.stage()==world_opening_prologue::Stage::MeadowExplore||
                opening_state.stage()==world_opening_prologue::Stage::Complete;
            update_camera_swimming(cam,water_controls_allowed);
            if(cam.swimming)cam.grounded=false;
            const bool smooth_player_walk=!cam.fly&&!cam.swimming&&cam.grounded&&fight.dodge_time<=0;
            if(!smooth_player_walk)player_move.reset();
            bool sprinting=keys[SDL_SCANCODE_LSHIFT]||(pad_playable&&preview_pad.sprint);
            float forward = float(keys[SDL_SCANCODE_W]) - float(keys[SDL_SCANCODE_S])+pad_forward, side = float(keys[SDL_SCANCODE_D]) - float(keys[SDL_SCANCODE_A])+pad_side;
            animation_polish_qa_controls(forward,side,sprinting);
            cam.yaw+=animation_polish_qa_yaw_rate()*dt;
            // Capture-only verification aid: hold forward after warm-up so
            // moving-actor rendering can be measured. Inert in ordinary play.
            if(launch->frames&&frame>120&&qa_autowalk)forward=1.f;
            // The apartment is a calm, tired walk: Shift, the controller
            // sprint button and a fully deflected stick all walk (1.25 m/s).
            const bool apartment_walk=opening_enabled&&!cam.fly&&!studio.open&&apartment_walk_speed>0&&
                (opening_state.stage()==world_opening_prologue::Stage::HouseExplore||opening_state.stage()==world_opening_prologue::Stage::GameMenu||
                 opening_state.stage()==world_opening_prologue::Stage::Title||opening_state.stage()==world_opening_prologue::Stage::HouseEstablish);
            if(apartment_walk)sprinting=false;
            float speed = cam.fly ? (studio.open?studio.fly_speed*(keys[SDL_SCANCODE_LSHIFT]?4.f:1.f):(keys[SDL_SCANCODE_LSHIFT] ? 700.f : 90.f)) : apartment_walk ? apartment_walk_speed : float(world_atlas_walk_speed()) * (sprinting ? 1.7f : 1.f);
            if(presentation&&!opening_enabled&&sprinting&&studio.components_runtime&&studio.components_runtime->counter(world_runtime_demo::renown_counter)>0)speed*=1.15f;
            if(fight.engaged()&&!weapon_sheathed)speed=arena_enabled?2.8f:2.35f;
            if(cam.swimming)speed=float(world_atlas_walk_speed())*world_swimming::speed_scale;
            float len = std::hypot(forward, side);
            if (len > 1) {
                forward /= len;
                side /= len;
            }
            V3 f = (cam.fly||(cam.swimming&&cam.dive.diving)) ? cam.forward() : V3{std::sin(cam.yaw), 0, -std::cos(cam.yaw)}, r = cam.right();
            const double move_vx=(f.x*forward+r.x*side)*speed,move_vz=(f.z*forward+r.z*side)*speed;
            // Captured civilian starts and stops request displacement before
            // the ordinary sweeps. Only the swept result reaches animation;
            // this never applies a second root transform after collision.
            const bool starting_jump=!fight.engaged()&&(opening_enabled?opening_jump:bool(keys[SDL_SCANCODE_SPACE]));
            const bool captured_walk_enabled=smooth_player_walk&&avatar_initialized&&player_move_initialized&&!player_motion_relocated&&
                story_civilian_ready&&skeletal_hero_loaded&&
                skeletal_hero.supports_unarmed_transitions()&&skeletal_hero_context_ready_now()&&
                skeletal_equipment_stowed()&&skeletal_hero.equipment_stowed_settled()&&
                !fight.engaged()&&!skeletal_terminal.active()&&!starting_jump;
            if(skeletal_hero_loaded){
                captured_walk_attempted=true;
                captured_walk_ok=skeletal_hero.plan_unarmed_motion(move_vx,move_vz,avatar_yaw,dt,
                    captured_walk_enabled,player_hitstop,captured_walk_request,skeletal_hero_error);
                if(!captured_walk_ok){std::fprintf(stderr,"[Story motion] %s\n",skeletal_hero_error.c_str());launch->exit_code=1;running=false;}
            }
            // The combat pose/edge already holds during impact pause. Keep its
            // controller root planted too; look input above remains live.
            const auto walk_delta=!captured_walk_ok?world_player_movement::Delta{}:
                captured_walk_request.captured?world_player_movement::Delta{captured_walk_request.dx,captured_walk_request.dz}:
                player_hitstop?player_move.step(move_vx,move_vz,dt,false):
                smooth_player_walk?player_move.step(move_vx,move_vz,dt):world_player_movement::Delta{move_vx*dt,move_vz*dt};
            const double move_start_x=cam.x,move_start_z=cam.z;
            double nx=std::clamp(cam.x+walk_delta.dx,-world_atlas_width()*.5+1,world_atlas_width()*.5-1);
            double nz=std::clamp(cam.z+walk_delta.dz,-world_atlas_depth()*.5+1,world_atlas_depth()*.5-1);
            const double combat_body_separation=skeletal_hero_loaded&&skeletal_hero.ready()&&skeletal_hero_context_ready_now()?1.20:1.35;
            if(fight.engaged()){
                double ox,oz;world_atlas_village_origin(&ox,&oz);
                if(presentation){ox=preview_arena_x;oz=preview_arena_z;}
                if(fight.dodge_time>0&&!player_hitstop){const double impulse=4.*std::sin(3.14159265358979323846*fight.dodge_time/.26);nx-=f.x*impulse*dt;nz-=f.z*impulse*dt;}
                if(playtest_enabled){if(!playtest_combat_can_step(nx,nz)){nx=cam.x;nz=cam.z;}}
                else if(first_journey_enabled){if(!first_journey_can_step(nx,nz)){nx=cam.x;nz=cam.z;}}
                else{nx=std::clamp(nx,ox-52.,ox-44.);nz=std::clamp(nz,oz-77.,oz-69.);}
                const double dx=nx-ox-fight.enemy_x,dz=nz-oz-fight.enemy_z,sep=std::hypot(dx,dz);
                if(!arena_enabled&&sep<combat_body_separation){if(sep>.00001){nx=ox+fight.enemy_x+dx/sep*combat_body_separation;nz=oz+fight.enemy_z+dz/sep*combat_body_separation;}else{nx=cam.x;nz=cam.z;}}
            }
            if(arena_enabled){
                std::array<world_arena_player_movement::Circle,32> bodies={};std::size_t body_count=0;
                for(const auto &actor:arena_game.challengers()){
                    if(actor.state==world_arena_combat::ChallengerState::Unused||actor.state==world_arena_combat::ChallengerState::Defeated)continue;
                    const bool approaching=actor.state==world_arena_combat::ChallengerState::Active||actor.state==world_arena_combat::ChallengerState::Entering;
                    const double measured_clearance=arena_game.bounds.player_radius+arena_game.opponent_movement_radius(unsigned(&actor-arena_game.challengers().data()));
                    const double radius=approaching?std::max(combat_body_separation,measured_clearance):std::max(.8,measured_clearance);
                    if(body_count<bodies.size())bodies[body_count++]={actor.position.x,actor.position.z,radius};
                }
                const auto constrained=world_arena_player_movement::move(
                    {cam.x-arena_origin_x,cam.z-arena_origin_z},{nx-arena_origin_x,nz-arena_origin_z},
                    bodies.data(),body_count,arena_game.bounds.half_extent-arena_game.bounds.player_radius);
                if(constrained.valid){nx=arena_origin_x+constrained.position.x;nz=arena_origin_z+constrained.position.z;arena_player_sweep_contacts+=constrained.contacts;}
                else{nx=cam.x;nz=cam.z;++arena_player_sweep_failures;}
            }
            auto moved=move_camera(cam,scenery,nx,nz);
            if(captured_walk_request.captured)player_move.reset();
            else if(smooth_player_walk)player_move.commit(walk_delta.dx,walk_delta.dz,cam.x-move_start_x,cam.z-move_start_z);
            manual_collision_steps+=moved.collision;manual_slope_steps+=moved.slope;manual_sliding_steps+=moved.sliding;
            update_camera_swimming(cam,water_controls_allowed);
            if (cam.fly) {
                const float pad_height=source_showcase&&pad_playable?preview_pad.right_trigger-preview_pad.left_trigger:0.f;
                cam.y += (float(keys[SDL_SCANCODE_E]) - float(keys[SDL_SCANCODE_Q])+pad_height) * speed * dt;
                cam.y = std::max(cam.y, double(walking_height(cam.x, cam.z, nullptr) + 1.75f));
            } else {
                const double support=walking_height(cam.x,cam.z,nullptr);
                const float floor=float(support)+1.75f;
                if(cam.swimming){
                    float surface=0,depth=0;
                    world_atlas_local_water(cam.x,cam.z,&surface,&depth);
                    const float down=(keys[SDL_SCANCODE_LCTRL]||keys[SDL_SCANCODE_RCTRL]||keys[SDL_SCANCODE_C])?1.f:0.f;
                    const float up=keys[SDL_SCANCODE_SPACE]?1.f:0.f;
                    const float pad_down=pad_playable?preview_pad.left_trigger:0.f,pad_up=pad_playable?preview_pad.right_trigger:0.f;
                    double vertical=(std::max(up,pad_up)-std::max(down,pad_down))*2.2;
                    if(cam.dive.diving)vertical+=std::sin(cam.pitch)*forward*speed;
                    const double target_eye=world_swimming::depth_target(cam.dive,cam.y,surface,support,vertical,dt);
                    const double requested=world_swimming::step(cam.y,target_eye,dt);
                    cam.vy=0;
                    move_camera_vertical(cam,scenery,requested,floor);
                    // A ceiling/obstacle cannot accumulate a hidden ascend or
                    // descend target that fires after the control is released.
                    if(cam.dive.diving&&std::abs(cam.y-requested)>.002)cam.dive.target_eye=cam.y;
                    cam.grounded=false;
                }else{
                    if (!fight.engaged()&&(opening_enabled?opening_jump:bool(keys[SDL_SCANCODE_SPACE])) && (cam.grounded||cam.y <= floor + .02))
                        cam.vy = 4.7f;
                    cam.vy -= 9.81f * dt;
                    move_camera_vertical(cam,scenery,cam.y+cam.vy*dt,floor);
                }
            }
        }

        if(skeletal_hero_loaded&&!captured_walk_attempted){
            captured_walk_ok=skeletal_hero.plan_unarmed_motion(0,0,avatar_yaw,dt,false,true,captured_walk_request,skeletal_hero_error);
            if(!captured_walk_ok){std::fprintf(stderr,"[Story motion] %s\n",skeletal_hero_error.c_str());launch->exit_code=1;running=false;}
        }
        player_move_last_x=cam.x;player_move_last_z=cam.z;player_move_initialized=true;
        joust_tick(launch->frames?1.f/60.f:dt);
        walking_controls_sample(frame,dt,player_controls_allowed);
        opening_jump=false;
        if (launch->tour && frame > 1 && frame % 360 == 0) {
            current = (current + 1) % int(landmark_count);
            teleport(cam, landmarks[current]);
            current_name=landmarks[current].name;destination_loading=true;
            temporal_cut=true;
            benchmark_x = cam.x;
            benchmark_z = cam.z;
        }

        // Rebuild a stale static lighting capture once an explicit look edit
        // finishes. All filtering stays in the existing loading phase; walking
        // with an unchanged look never bakes or reads back radiance.
        const int changed_probe_region=probe_region(cam);
        if(!launch->frames&&!launch->benchmark&&!show_graphics&&!studio.open&&
           !ImGui::IsAnyItemActive()&&changed_probe_region>=0&&local_probes_enabled&&
           local_probe_regions[changed_probe_region]&&!local_probe_light_matches(changed_probe_region))
            destination_loading=true;
        if(playtest_qa){
            playtest_drive(frame);
        } else if(source_review.active) {
            const bool ready=!destination_loading&&terrain.view_ready(cam.x,cam.z,true)&&world_scenery_streaming_stats(scenery).critical_pending==0;
            source_review_waiting_frame=!ready||frame<=uint64_t(launch->warmup);
            if(!ready)destination_loading=true;
            const bool cut=source_review.advance(cam,ready,frame,unsigned(launch->warmup));
            const bool probe_domain=probe_region(cam)==2&&local_probes_enabled&&
                (!local_probe_regions[2]||std::hypot(cam.x-mountain_probe_destination[0],cam.z-mountain_probe_destination[2])>=48.||std::abs(cam.y-mountain_probe_destination[1])>=24.);
            if(cut||probe_domain){destination_loading=true;source_review_waiting_frame=true;if(cut)temporal_cut=true;}
            current_name=source_review.label();
        } else if (launch->benchmark && !(qa_runtime&&qa_runtime_phase>=8) && !(qa_studio_tools && std::getenv("VALWOOD_QA_STUDIO_INTERACTION") && qa_studio_phase>=5)) {
            float t = float(frame % 720) / 720.f, progress = t < .5f ? t * 2 : (1 - t) * 2;
            if(starting_route.active) {
                if(frame<=uint64_t(launch->warmup)||destination_loading||!terrain.view_ready(cam.x,cam.z,false))starting_route.waiting_frames++;
                else starting_route.advance(cam,scenery);
                if(starting_route.milestone==4&&std::getenv("VALWOOD_QA_STRONGHOLD_SNOW")&&starting_route.walking_frames%120==0&&(starting_route.collision_blocked||starting_route.slope_blocked)){
                    std::printf("[Grandstone route] QA rehearsal stopped on obstructed segment %zu; collision %zu slope %zu\n",starting_route.next,starting_route.collision_blocked,starting_route.slope_blocked);std::fflush(stdout);running=false;
                }
                if(stronghold_finish&&starting_route.complete&&std::getenv("VALWOOD_QA_STRONGHOLD_SNOW")){cam.yaw=starting_route.milestone==5?3.14159265f:0;cam.pitch=-.92f;}
            } else if(pocket_benchmark) {
                cam.x=benchmark_x;cam.z=benchmark_z;cam.fly=false;
                cam.y=walking_height(cam.x,cam.z,nullptr)+1.75;
                cam.yaw=benchmark_yaw+(!std::strcmp(launch->view,"sweep") ? t*6.2831853f : .12f*std::sin(t*6.2831853f));
            } else {
                cam.x = benchmark_x + progress * 145.;
                cam.z = benchmark_z - progress * 90.;
                cam.y = std::max(world_atlas_height(cam.x, cam.z), 0.f) + (cam.fly ? 300. : 1.75);
                cam.yaw = !std::strcmp(launch->view, "sweep") ? t * 6.2831853f : benchmark_yaw + .38f * std::sin(t * 6.2831853f);
            }
        }

        // Explicit native integration check uses the same destination path as
        // H travel, exercising release AFTER a farm bank has been resident.
        if((qa_wildlife_return||qa_bandit_return)&&frame==480) {
            for(unsigned i=0;i<std::size(scenic_views);i++)if(!std::strcmp(scenic_views[i].id,"castle-market")) {
                scenic_travel(cam,i);current_name=scenic_views[i].name;destination_loading=true;temporal_cut=true;
                benchmark_x=cam.x;benchmark_z=cam.z;benchmark_yaw=cam.yaw;break;
            }
        }

        float visual_time=qa_fixed_time?22.f:(launch->frames ? 17.f+float(frame)/60.f : float(SDL_GetTicks64()*.001));
        final_visual_time=visual_time;studio_surface_gpu.wind_time=visual_time;
        world_stronghold_update(stronghold,visual_time);
        world_snow_update(snow,visual_time,cam.x,cam.y-1.75,cam.z,cam.grounded&&!cam.fly&&!destination_loading&&(launch->benchmark||!preview_blocked()),stronghold_x,stronghold_z);

        #include "world_runtime_service.inc"
        if(active_playtest_walk)playtest_walk_reference=cam.y-1.75;
        if(equipment_tick)equipment_tick(dt,!player_controls_allowed);
        if(weapon_sheathed||equipment_change.active()||(skeletal_hero_loaded&&skeletal_hero.supports_equipment()&&!skeletal_hero.equipment_combat_ready())){fight.set_guard(false);fight.clear_buffer();}
        if(!arena_roster_qa&&!blade_clash_qa)arena_controls(player_controls_allowed&&!arena_finished());
        if(arena_enabled){
            const bool aim=arena_bow()&&!weapon_sheathed&&(arena_mouse_aim||(preview_pad.using_pad&&preview_pad.left_trigger>.4f));
            auto focus=arena_game.active_position();
            if(arena_target_lock.locked())focus=arena_game.challengers()[arena_target_lock.target()].position;
            const double distance=std::hypot(arena_origin_x+focus.x-cam.x,arena_origin_z+focus.z-cam.z);
            const bool melee_lens=third_person&&fight.engaged()&&!arena_bow()&&!weapon_sheathed&&
                !preview_settings.reduced_motion&&!arena_look_held();
            const float wanted=aim?.78f:(melee_lens?world_combat_close_melee_fov(distance):1.08f);
            cam.fov+=(wanted-cam.fov)*(1-std::exp(-10.f*std::min(dt,.05f)));
        }
        if(!authored_cutscene_locked&&!cutscene_return_neutral){
            if(!joust_enabled)preview_tick(dt);
            // Opening events belong to this tick. A paused authored take must
            // never replay the previous Started event or reset its save again.
            if(opening_enabled&&(opening_frame.events&world_opening_prologue::Started)&&cutscene_progress_reset)cutscene_progress_reset();
        }
        npc_intro_qa_observe(frame);
        road_traffic_tick(launch->frames?1.f/60.f:dt);
        world_village_population_update(population,cam.x,cam.y,cam.z,visual_time,qa_fixed_time?0.f:(launch->frames?1.f/60.f:dt));
        population_peak_visible=std::max(population_peak_visible,world_village_population_stats(population).visible);
        const bool fight_was_engaged=fight.engaged();
        double fight_ox,fight_oz;world_atlas_village_origin(&fight_ox,&fight_oz);
        if(presentation){fight_ox=preview_arena_x;fight_oz=preview_arena_z;}
        if(!arena_enabled)fight.weapon=story_weapon;
        // A bounded first-person capture starts the ordinary private arena
        // after destination loading, so the test exercises the combat arms
        // path rather than the setup menu where held weapons are hidden.
        if((force_capture_first||force_capture_arena)&&arena_enabled&&!destination_loading&&!rig_capture_started&&arena_begin()){
            third_person=!force_capture_first;rig_capture_started=true;
        }
        orc_roster_qa_step(frame);
        feedback_qa_step(frame);
        if(animation_polish_qa_step(frame))blade_clash_qa_step(frame);
        roster_qa_step(frame);
        sync_skeletal_hero_combat_timing();
        if(inventory_enabled)fight.gear_resistance=1.f-world_player_inventory::protection(player_inventory);
        const bool fight_paused=(battlefield_frame.active&&battlefield_frame.cinematic)||authored_cutscene_locked||cutscene_return_neutral||(arena_enabled&&!arena_controls_ready())||(first_journey_enabled?(preview_screen!=1||first_journey_dialogue_active):preview_blocked())||(studio.components_runtime&&studio.components_runtime->current_line())||studio.production_freeze()||studio.cinematic_preview||show_map||show_graphics||destination_loading||(!qa_training&&!qa_preview&&!qa_controller&&!playtest_combat_qa&&!qa_stick_combat&&!combat_feedback_qa&&!arena_roster_qa&&!blade_clash_qa&&!equipment_qa&&!duel_qa&&!(SDL_GetWindowFlags(window)&SDL_WINDOW_INPUT_FOCUS));
        if(fight.engaged()&&fight_paused)fight_paused_frames++;
        fight.impact_pause_enabled=!presentation||!preview_settings.reduced_motion;
        fight.contact_query=arena_opponent_anatomy_ready()?+[](const TrainingAnatomicalQuery &query,TrainingAnatomicalHit &hit,void *context){
            return (*static_cast<std::function<bool(const TrainingAnatomicalQuery &,TrainingAnatomicalHit &)>*>(context))(query,hit);
        }:nullptr;
        fight.contact_query_context=arena_opponent_anatomy_ready()?&combat_anatomical_query:nullptr;
        const unsigned fight_old_swings=fight.player_swings,fight_old_enemy_swings=fight.enemy_swings;
        const unsigned fight_old_hits=fight.hits,fight_old_enemy_hits=fight.enemy_hits,fight_old_parries=fight.parries,fight_old_blocks=fight.blocks;
        const float fight_old_health=fight.health,fight_old_enemy_health=fight.enemy_health;
        const auto fight_old_severed_parts=fight.enemy_severed_parts;
        ordinary_contact_events.clear();fight.contact_events=&ordinary_contact_events;
        skeletal_terminal.begin_frame();
        first_journey_combat_step(launch->frames?1.f/60.f:dt,fight_paused);
        if(first_journey_enabled){fight_ox=preview_arena_x;fight_oz=preview_arena_z;}
        // The arena advances the one shared player clock and every active
        // opponent together. Ordinary one-on-one encounters retain this path.
        if(!arena_enabled)fight.update(launch->frames?1./60.:dt,cam.x-fight_ox,cam.z-fight_oz,fight_paused);
        const double arena_result_frame_dt=std::min(double(launch->frames?1.f/60.f:dt),.25);
        const float arena_result_visual_dt=float(arena_enabled&&arena_ui_phase==ArenaUiPhase::Results&&arena_game.state()==world_arena_combat::State::Won?
            std::max(0.,arena_result_frame_dt-std::max(0.,fight.impact_pause)):0.);
        // Keep ordinary AI inside the currently displayed rig's actual sword
        // arc. This changes movement spacing, never damage or contact reach.
        arena_game.set_approach_radius(skeletal_hero_loaded&&skeletal_hero.ready()&&skeletal_hero_context_ready_now()?1.25:2.05);
        arena_tick(launch->frames?1.f/60.f:dt,fight_paused);
        // Results pause gameplay, while the committed winning swing may finish.
        // A real menu/focus/loading pause still freezes this visual continuation.
        const bool skeletal_result_animation=arena_enabled&&arena_ui_phase==ArenaUiPhase::Results&&
            arena_game.state()==world_arena_combat::State::Won&&!arena_results_paused();
        const bool skeletal_visual_paused=fight_paused&&!skeletal_result_animation;
        skeletal_terminal.update(fight,launch->frames?1.f/60.f:dt,skeletal_visual_paused,
            skeletal_hero_loaded&&skeletal_hero.ready()&&skeletal_hero_context_ready_now()&&!studio.open&&!destination_loading,
            fight.hits>fight_old_hits);
        if(arena_enabled){fight_ox=preview_arena_x;fight_oz=preview_arena_z;}
        if(fight.enemy_hits>fight_old_enemy_hits)skeletal_hero_hit_serial+=fight.enemy_hits-fight_old_enemy_hits;
        // One authoritative result path drives the HUD, audio and controller.
        // Aggregate misses includes steel blocks, so it cannot classify a whiff.
        if(combat_feedback_clock>=0&&fight.clock+1e-6<combat_feedback_clock){
            combat_feedback.reset();combat_feedback_legacy_serial=combat_feedback_legacy_attack=0;
        }
        combat_feedback_clock=fight.clock;
        combat_feedback.advance(launch->frames?1./60.:dt,fight.clock,fight_paused);
        if(fight_paused){
            training_sounds.mute();
            if(!combat_feedback_was_paused)preview_pad.stop_rumble();
        }
        combat_feedback_was_paused=fight_paused;
        const world_combat_feedback::Preferences feedback_preferences{preview_settings.vibration,preview_settings.reduced_motion};
        auto present_contact=[&](const TrainingContactEvent &event){
            world_combat_feedback::Cue cue;
            if(!combat_feedback.observe(event,feedback_preferences,cue))return;
            float pan=0,gain=1;
            if(arena_enabled&&!world_combat_feedback::local_air(event.kind)&&event.opponent_id<arena_game.config().total){
                const auto &actor=arena_game.challengers()[event.opponent_id];
                const double dx=arena_origin_x+actor.position.x-cam.x,dz=arena_origin_z+actor.position.z-cam.z;
                const double distance=std::hypot(dx,dz);const auto right=cam.right();
                pan=float(std::clamp((dx*right.x+dz*right.z)/std::max(.01,distance),-.85,.85));
                gain=float(std::clamp(1.-std::max(0.,distance-2.)/16.,.12,1.));
            }
            if(cue.sound>=0&&training_sounds.play(unsigned(cue.sound),cue.volume,pan,gain,cue.sound_fade_ms))++combat_feedback_audio_dispatches;
            if(presentation&&cue.kind==world_combat_feedback::Kind::Incoming)preview_hit=.35f;
            if(presentation&&cue.kind==world_combat_feedback::Kind::Parry)preview_parry=.3f;
        };
        {
            const auto &events=arena_enabled?arena_game.events():ordinary_contact_events;
            for(unsigned n=0;n<events.count;++n)present_contact(events.values[n]);
        }
        world_combat_feedback::Cue haptic_command;
        if(combat_feedback.take_haptic_command(feedback_preferences,haptic_command)){
            if(haptic_command.rumble_ms){preview_pad.rumble(haptic_command.low,haptic_command.high,haptic_command.rumble_ms,true);++combat_feedback_haptic_dispatches;}
            else preview_pad.stop_rumble();
        }
        if(fight.state==TrainingState::Fighting&&!fight_paused){const unsigned arena=fight.trail_threat?1:0;if(++combat_timing_warmup[arena]>120){if(!combat_timing_windows[arena][0])combat_timing_windows[arena][0]=frame;combat_timing_windows[arena][1]=frame;}}
        if(!arena_enabled&&!first_journey_enabled&&fight.engaged()&&!fight_paused){cam.yaw=float(std::atan2(fight_ox+fight.enemy_x-cam.x,-(fight_oz+fight.enemy_z-cam.z)));cam.pitch=-.12f;}
        if(fight_was_engaged&&!fight.engaged()){
            fight_end_frame=frame;
            // The road encounter returns directly to exploration. Its input
            // owner releases the mouse for dialogue, defeat and pause screens.
            if(!first_journey_enabled){look_active=false;SDL_SetRelativeMouseMode(SDL_FALSE);}
        }
        const bool fight_result_in_lane=(fight.state==TrainingState::Won||fight.state==TrainingState::Lost||fight.state==TrainingState::Cancelled)&&std::hypot(cam.x-fight_ox-fight.enemy_x,cam.z-fight_oz-fight.enemy_z)<8.;
        // The paid rig is the persistent opponent, including before E/retries.
        // Reserve its old partner's half of the interactive lane permanently.
        const bool owned_enemy_near=std::hypot(cam.x-fight_ox-fight.enemy_x,cam.z-fight_oz-fight.enemy_z)<180.;
        world_castle_training_sparring(training,combat_motion!=nullptr&&(!presentation||!fight.trail_threat),fight.engaged()?fight.enemy_animation():0.f);
        world_castle_training_replace_opponent(training,combat_motion!=nullptr&&(!presentation||!fight.trail_threat));
        world_castle_training_update(training,cam.x,cam.y,cam.z,visual_time,qa_fixed_time?0.f:(launch->frames?1.f/60.f:dt));
        training_peak_visible=std::max(training_peak_visible,world_castle_training_stats(training).visible);
        world_village_watch_update(watch,cam.x,cam.y,cam.z,visual_time,qa_fixed_time?0.f:(launch->frames?1.f/60.f:dt));
        watch_peak_visible=std::max(watch_peak_visible,world_village_watch_stats(watch).visible);
        world_village_pets_update(pets,cam.x,cam.y,cam.z,visual_time,qa_fixed_time?0.f:(launch->frames?1.f/60.f:dt));
        pets_peak_visible=std::max(pets_peak_visible,world_village_pets_stats(pets).visible);
        WorldVillageResident qa_actor;
        if(qa_npc_follow<world_village_resident_count&&world_village_population_resident(population,qa_npc_follow,&qa_actor)) {
            // Inspect civilians from the open aisle side, keeping the cargo
            // beside their market turnaround out of the foot-level view.
            const float side=qa_npc_follow<8?3.f:-3.f;
            cam.x=qa_actor.x+side;cam.z=qa_actor.z+4.5;cam.y=qa_actor.y+1.95;
            cam.yaw=std::atan2(-side,4.5f);cam.pitch=std::atan2(-.95f,std::hypot(side,4.5f));
            cam.vy=0;cam.fly=false;
        }
        WorldVillagePet qa_pet;
        if(qa_pet_follow<world_village_pet_count&&world_village_pets_pet(pets,qa_pet_follow,&qa_pet)) {
            cam.x=qa_pet.x+2.2;cam.z=qa_pet.z+3.;cam.y=qa_pet.y+.95;
            cam.yaw=std::atan2(-2.2f,3.f);cam.pitch=std::atan2(-.60f,std::hypot(2.2f,3.f));
            cam.vy=0;cam.fly=false;
        }
        if((qa_graphics_cycle||qa_display_cycle)&&(frame==180||frame==360||frame==540)) {
            graphics_pending=world_graphics_settings_default();graphics_pending.auto_quality=false;
            if(frame==180){world_graphics_settings_apply_preset(&graphics_pending,WorldGraphicsPreset::Performance);
                graphics_pending.render_scale=.75f;graphics_pending.clouds=false;graphics_pending.volumetric_fog=false;graphics_pending.reflections=false;}
            if(frame==360){world_graphics_settings_apply_preset(&graphics_pending,WorldGraphicsPreset::Cinematic);graphics_pending.render_scale=.90f;}
            if(qa_display_cycle){
                graphics_pending.window_width=frame==180?1280:1600;graphics_pending.window_height=frame==180?720:900;
                graphics_pending.display_mode=frame==360?WorldDisplayMode::Borderless:WorldDisplayMode::Windowed;
                graphics_pending.vsync=frame!=180;
            }
            graphics_apply=true;
        }
        if(foliage_cycle_dir&&foliage_cycle_advance){
            foliage_cycle_advance=false;graphics_pending=graphics;
            graphics_pending.foliage_density=WorldFoliageDensity(foliage_cycle_phase%4);
            graphics_pending.preset=WorldGraphicsPreset::Custom;graphics_apply=true;
        }
        adaptive_queries.poll(quality);
        const bool can_resize=!destination_loading&&terrain.view_ready(cam.x,cam.z,cam.fly);
        if(graphics_apply&&can_resize) {
            graphics_apply=false;
            int old_window_w=0,old_window_h=0;SDL_GetWindowSize(window,&old_window_w,&old_window_h);
            auto previous_display=graphics;
            previous_display.window_width=old_window_w;previous_display.window_height=old_window_h;
            const bool display_ok=graphics_display(window,graphics_pending);
            SDL_GL_GetDrawableSize(window,&display_w,&display_h);SDL_GetWindowSize(window,&logical_w,&logical_h);refresh_designer_view();
            QualityController staged_quality;
            staged_quality.reset({graphics_pending.auto_quality,graphics_pending.render_scale,graphics_pending.min_render_scale,float(world_graphics_effective_target_fps(graphics_pending,graphics_refresh_hz(window)))},quality_epoch+1);
            const float staged_scale=graphics_pending.auto_quality?staged_quality.scale():graphics_pending.render_scale;
            const int rw=std::max(1,int(std::lround(scene_display_w*staged_scale))),rh=std::max(1,int(std::lround(scene_display_h*staged_scale)));
            Target staged_target;staged_target.separate_indirect=target.separate_indirect;
            Shadow staged_shadow;Target staged_reflection;
            const auto staged_water=world_water_quality_profile(graphics_pending.water_quality);
            const bool staged_planar=graphics_pending.reflections&&staged_water.planar;
            const float staged_mirror_scale=std::min(graphics_pending.reflection_scale,staged_water.mirror_scale_cap);
            const int mirror_w=std::max(1,int(std::lround(rw*staged_mirror_scale))),mirror_h=std::max(1,int(std::lround(rh*staged_mirror_scale)));
            const bool change_reflection=staged_planar&&(!reflection.fbo||mirror_w!=reflection.w||mirror_h!=reflection.h);
            const bool change_target=rw!=target.w||rh!=target.h;
            const bool change_shadow=graphics_pending.shadow_side!=shadow.side;
            const bool allocated=display_ok&&(!change_target||staged_target.resize(rw,rh))&&(!change_shadow||staged_shadow.resize(graphics_pending.shadow_side))&&(!change_reflection||staged_reflection.resize(mirror_w,mirror_h))&&(!blood_renderer||!change_target||world_blood_prepare(blood_renderer,rw,rh))&&(!dressing.cover||world_cover_set_quality(dressing.cover,int(graphics_pending.foliage_density)));
            bool committed=false;
            if(!allocated){
                quality_apply_failures++;
                std::snprintf(graphics_message,sizeof(graphics_message),"Could not allocate this quality level. Previous settings retained.");
            }else if(world_graphics_settings_save(graphics_settings_path,graphics_pending,graphics_message,sizeof(graphics_message))){
                committed=true;
                if(change_target){target.release();target=staged_target;staged_target={};quality_reallocations++;}
                if(change_shadow){glDeleteFramebuffers(1,&shadow.fbo);glDeleteTextures(1,&shadow.depth);shadow.fbo=staged_shadow.fbo;shadow.depth=staged_shadow.depth;shadow.side=staged_shadow.side;staged_shadow.fbo=staged_shadow.depth=0;}
                if(change_reflection){reflection.release();reflection=staged_reflection;staged_reflection={};}
                graphics_apply_count++;graphics=graphics_pending;foliage_detail_scale=world_foliage_distance_scale(graphics.foliage_distance);foliage_density_scale=world_foliage_density_scale(graphics.foliage_density);foliage_budget_tier=graphics.foliage_density;rendered_scale=staged_scale;quality=staged_quality;quality_epoch++;
                latest_color=target.color;temporal_cut=true;quality_valid_after=frame+60;display_resize=false;
                minimum_rendered_scale=std::min(minimum_rendered_scale,rendered_scale);maximum_rendered_scale=std::max(maximum_rendered_scale,rendered_scale);
                if(!staged_planar)reflection.release();
                std::printf("[Graphics] Water quality %s: planar%d SSR%d/%d filtered%d\n",world_water_quality_name(graphics.water_quality),int(staged_planar),graphics.reflections?staged_water.ssr_steps:0,staged_water.ssr_refine_steps,int(staged_water.filtered));
                std::printf("[Graphics] Applied %s, scene%dx%d shadow%d reflection%.2f enabled%d contact%d fog%d clouds%d\n",
                    world_graphics_preset_name(graphics.preset),target.w,target.h,shadow.side,graphics.reflection_scale,int(graphics.reflections),int(graphics.contact_shadows),int(graphics.volumetric_fog),int(graphics.clouds));
                std::printf("[Graphics] Display %s %dx%d VSync %d\n",graphics.display_mode==WorldDisplayMode::Borderless?"Borderless":"Windowed",display_w,display_h,SDL_GL_GetSwapInterval());
                std::printf("[Graphics] Foliage draw distance %s (%.2fx)\n",world_foliage_distance_name(graphics.foliage_distance),foliage_detail_scale);
                std::snprintf(graphics_message,sizeof(graphics_message),graphics.character_textures==resident_character_textures&&graphics.environment_textures==resident_environment_textures?"Applied and saved.":"Applied and saved. Restart the game to load the new character or environment texture quality.");
            }
            if(!committed){
                // Apply is atomic: a failed save also restores the old generation policy.
                if(dressing.cover&&!world_cover_set_quality(dressing.cover,int(graphics.foliage_density))){launch->exit_code=1;running=false;}
                if(blood_renderer&&!world_blood_prepare(blood_renderer,target.w,target.h)){std::fprintf(stderr,"[Blood] Could not restore resolution resources\n");launch->exit_code=1;running=false;}
                graphics_display(window,previous_display);SDL_GL_GetDrawableSize(window,&display_w,&display_h);SDL_GetWindowSize(window,&logical_w,&logical_h);refresh_designer_view();
            }
            actual_swap_interval=SDL_GL_GetSwapInterval();graphics_monitor_refresh=graphics_refresh_hz(window);
            complete_before_swap=actual_swap_interval>0&&!presentation&&!(launch->frames&&qa_disable_completion&&!std::strcmp(qa_disable_completion,"1"));
            staged_target.release();staged_shadow.release();staged_reflection.release();
        }
        // A directed introduction renders at the display's native resolution.
        // This is temporary: saved graphics/adaptive preferences stay intact.
        const bool npc_intro_native_resolution=opening_enabled&&opening_frame.npc_intro&&
            !(opening_intro_media.ready()&&!opening_intro_media.failed()&&opening_intro_media.mode()==world_npc_intro_media::Mode::Video)&&!opening_intro_resolution_failed;
        const float requested_scale=npc_intro_native_resolution?1.f:(graphics.auto_quality?quality.scale():graphics.render_scale);
        if(can_resize&&(display_resize||std::fabs(requested_scale-rendered_scale)>.001f)) {
            const int rw=std::max(1,int(std::lround(scene_display_w*requested_scale))),rh=std::max(1,int(std::lround(scene_display_h*requested_scale)));
            const int previous_width=target.w,previous_height=target.h;
            if((!blood_renderer||world_blood_prepare(blood_renderer,rw,rh))&&target.resize(rw,rh)){
                rendered_scale=requested_scale;display_resize=false;latest_color=target.color;
                quality.reconfigure(++quality_epoch);quality_valid_after=frame+30;quality_reallocations++;temporal_cut=true;
                minimum_rendered_scale=std::min(minimum_rendered_scale,rendered_scale);maximum_rendered_scale=std::max(maximum_rendered_scale,rendered_scale);
                std::printf("[Graphics] Scene %dx%d -> display %dx%d, scale%.2f, epoch%llu\n",rw,rh,display_w,display_h,rendered_scale,(unsigned long long)quality_epoch);
            }else{
                if(blood_renderer&&!world_blood_prepare(blood_renderer,previous_width,previous_height)){std::fprintf(stderr,"[Blood] Could not restore resolution resources\n");launch->exit_code=1;running=false;}
                if(npc_intro_native_resolution){
                    opening_intro_resolution_failed=true;display_resize=false;quality_apply_failures++;
                    std::fprintf(stderr,"[NPC intro] Native scene resolution unavailable; retaining the player's supported resolution.\n");
                }else{
                display_resize=false;quality_apply_failures++;graphics.auto_quality=false;graphics.render_scale=rendered_scale;
                graphics.min_render_scale=std::min(graphics.min_render_scale,rendered_scale);graphics.preset=WorldGraphicsPreset::Custom;graphics_draft=graphics;
                quality.reset({false,rendered_scale,graphics.min_render_scale,float(world_graphics_effective_target_fps(graphics,graphics_refresh_hz(window)))},++quality_epoch);
                std::snprintf(graphics_message,sizeof(graphics_message),"Image resize unavailable. Automatic quality paused; previous resolution retained.");
                }
            }
        }
        cam.aspect=world_designer_viewport::aspect(designer_view);
        studio.cinematic_camera(cam,SDL_GetTicks64()*.001);if(studio.cinematic_cut){temporal_cut=true;studio.cinematic_cut=false;}
        terrain.update(cam.x, cam.z, 2);
        const bool grass_gameplay=player_controls_allowed&&!cam.fly&&!source_review.active&&
            !(opening_enabled&&(opening_frame.cinematic||opening_frame.camp_dialogue||opening_frame.stage==world_opening_prologue::Stage::Title||opening_frame.stage==world_opening_prologue::Stage::GameMenu));
        grass_interaction.update(dt,cam.x,cam.y-1.75,cam.z,grass_gameplay,cam.grounded);
        visual_grass_interactor=grass_interaction.head;
        Camera render_cam=cam;
        active_follow_camera=third_person&&!cam.fly;
        float follow_distance=0,follow_lens_radius=.22f;double follow_pivot_y=cam.y;
        bool follow_line_blocked=false,follow_motion_blocked=false,follow_view_clear=true,follow_recovering=false;
        const auto *preview_line=presentation&&studio.components_runtime?studio.components_runtime->current_line():nullptr;
        const bool opening_camera_locked=authored_cutscene_locked||(opening_enabled&&(opening_frame.cinematic||opening_frame.camp_dialogue||opening_frame.stage==world_opening_prologue::Stage::Title||opening_frame.stage==world_opening_prologue::Stage::GameMenu));
        if(preview_line&&!opening_enabled){
            follow_camera_safety.reset();
            character_step_height.valid=false;
            combat_shoulder_active=false;combat_shoulder_offset=0;
            world_runtime::Entity speaker;const auto id=preview_line->id==730011?world_runtime_demo::instructor_id:world_runtime_demo::jungle_actor_first;
            if(studio.components_runtime->entity(id,speaker)){
                Camera goal=render_cam;const double dx=speaker.pose.position.x-goal.x,dz=speaker.pose.position.z-goal.z,distance=std::max(.01,std::hypot(dx,dz));
                const double framing=1.85*std::clamp(double(speaker.scale),.85,1.2),px=speaker.pose.position.x-dx/distance*framing,pz=speaker.pose.position.z-dz/distance*framing;
                goal.y=speaker.pose.position.y+1.62*speaker.scale;
                if(world_scenery_can_move(scenery,goal.x,goal.z,px,pz,float(goal.y)-.15f,.3f,.16f)){goal.x=px;goal.z=pz;}
                const double gx=speaker.pose.position.x-goal.x,gz=speaker.pose.position.z-goal.z;
                goal.yaw=float(std::atan2(gx,-gz));goal.pitch=float(std::atan2(speaker.pose.position.y+1.48*speaker.scale-goal.y,std::max(.1,std::hypot(gx,gz))));goal.fov=.76f;
                if(!dialogue_camera_active||temporal_cut){dialogue_camera=goal;temporal_cut=true;}
                else{const double t=1-std::exp(-std::clamp(double(dt),0.,.1)*7);dialogue_camera.x+=(goal.x-dialogue_camera.x)*t;dialogue_camera.y+=(goal.y-dialogue_camera.y)*t;dialogue_camera.z+=(goal.z-dialogue_camera.z)*t;dialogue_camera.yaw+=float(std::remainder(double(goal.yaw-dialogue_camera.yaw),6.28318530718)*t);dialogue_camera.pitch+=float((goal.pitch-dialogue_camera.pitch)*t);dialogue_camera.fov+=float((goal.fov-dialogue_camera.fov)*t);dialogue_camera.aspect=goal.aspect;}
                render_cam=dialogue_camera;dialogue_camera_active=true;
            }
        }else if(third_person&&!cam.fly){
            dialogue_camera_active=false;
            if(temporal_cut)character_step_height.valid=false;
            const double follow_anchor_y=character_step_height.update(cam.x,cam.y,cam.z,fight_paused?0:dt,cam.grounded,
                skeletal_hero_loaded&&skeletal_hero_supported_now()&&!studio.open&&!destination_loading,
                [&](double from,double to){
                    if(std::abs(to-from)<1.e-7)return true;
                    const float low=float(std::min(from,to)-.18),height=float(std::abs(to-from)+.36);
                    if(!world_scenery_can_move(scenery,cam.x,cam.z,cam.x,cam.z,low,height,.22f))return false;
                    if(active_playtest_walk||active_city_physics){
                        ++source_camera_casts;world_physics::Hit hit;
                        const world_physics::Vec3 center{cam.x,from,cam.z},delta{0,to-from,0};
                        const bool queried=active_playtest_walk?active_playtest_walk->cast_capsule(center,.22f,.02f,delta,hit):active_city_physics->cast_capsule(center,.22f,.02f,delta,hit);
                        if(!queried)++source_camera_query_failures;
                        if(!queried||(hit.hit&&hit.fraction<.999f)){++source_camera_blocks;return false;}
                    }
                    return true;
                });
            const bool closeup_context=(presentation||world_exploration)&&!preview_settings.reduced_motion&&fight.weapon!=world_weapon_loadout::Weapon::Bow&&!cam.swimming&&
                !studio.open&&!preview_line&&!opening_camera_locked&&!milestone_review.active()&&!source_review.active&&!destination_loading&&
                !authored_cutscene_locked;
            const bool closeup_engaged=fight.engaged()&&!weapon_sheathed&&(!arena_enabled||!arena_look_held());
            // Leaving combat/stowing/free-look fades the profile back out.
            // First person, bow, swim and reduced-motion contexts retain the
            // accepted camera directly; their gameplay aim is never changed.
            double closeup_target_x=preview_arena_x+fight.enemy_x,closeup_target_z=preview_arena_z+fight.enemy_z;
            if(arena_enabled&&arena_target_lock.locked()){
                const auto &target=arena_game.challengers()[arena_target_lock.target()];
                closeup_target_x=arena_origin_x+target.position.x;closeup_target_z=arena_origin_z+target.position.z;
            }
            const double closeup_distance=closeup_engaged?std::hypot(closeup_target_x-cam.x,closeup_target_z-cam.z):3.3;
            unsigned closeup_nearby=0;
            if(arena_enabled&&closeup_engaged)for(unsigned i=0;i<arena_game.config().total;++i){const auto &a=arena_game.challengers()[i];const auto *o=arena_game.opponent(i);
                if(a.state!=world_arena_combat::ChallengerState::Active||!o||o->health<=0)continue;
                if(std::hypot(arena_origin_x+a.position.x-cam.x,arena_origin_z+a.position.z-cam.z)<=6.5)++closeup_nearby;
            }
            const auto closeup=combat_closeup_camera.update(closeup_distance,fight_paused?0.f:dt,closeup_context,temporal_cut,std::max(1u,closeup_nearby));
            const bool exploration_framing=(presentation||world_exploration)&&!arena_enabled&&!joust_enabled&&
                !preview_settings.reduced_motion&&!fight.engaged()&&!cam.swimming&&
                (fight.weapon!=world_weapon_loadout::Weapon::Bow||weapon_sheathed)&&
                !studio.open&&!preview_line&&!opening_camera_locked&&!milestone_review.active()&&!source_review.active&&!destination_loading;
            const bool exploration_indoors=world_opening_interior::contains(opening_interior,{cam.x,cam.y,cam.z});
            const auto exploration=exploration_camera.update(exploration_framing,exploration_indoors,closeup.boom,fight_paused?0.f:dt,temporal_cut);
            const auto follow=world_follow_camera(cam.x,follow_anchor_y,cam.z,cam.yaw,cam.pitch,boom_distance,dt,temporal_cut,
                [&](double x,double z){
                    double floor=walking_height(x,z,nullptr);float surface=0;
                    // Contract the same collision-checked boom before it dips
                    // below the surface while the player is floating.
                    if(cam.swimming&&!cam.dive.diving&&world_atlas_local_water(x,z,&surface,nullptr))floor=std::max(floor,double(surface)+.05);
                    return floor;
                },
                [&](double ax,double az,double bx,double bz,float y,float h,float radius){
                    if(!world_scenery_can_move(scenery,ax,az,bx,bz,y,h,radius))return false;
                    if(active_playtest_walk||active_city_physics){
                        ++source_camera_casts;
                        world_physics::Hit hit;const world_physics::Vec3 center{ax,y+h*.5,az},delta{bx-ax,0,bz-az};
                        const float half=world_follow_camera_capsule_half_height(h,radius);
                        const bool queried=active_playtest_walk?active_playtest_walk->cast_capsule(center,radius,half,delta,hit):active_city_physics->cast_capsule(center,radius,half,delta,hit);
                        if(!queried)++source_camera_query_failures;
                        if(!queried||(hit.hit&&hit.fraction<.999f)){++source_camera_blocks;return false;}
                    }
                    return true;
                },exploration.boom,closeup.lift,exploration.pivot_drop);
            render_cam.x=follow.x;render_cam.y=follow.y;render_cam.z=follow.z;follow_distance=boom_distance=follow.distance;
            combat_shoulder_active=presentation&&fight.engaged()&&!arena_bow()&&follow.distance>.75f&&
                (!arena_enabled||!arena_look_held());
            const bool exploration_shoulder=exploration.shoulder>.001f&&!opening_camera_locked&&!cam.swimming&&!studio.open;
            const auto shoulder=world_combat_shoulder_camera(render_cam.x,render_cam.y,render_cam.z,cam.yaw,combat_shoulder_offset,follow.distance,dt,combat_shoulder_active||exploration_shoulder,temporal_cut,
                [&](double x,double z){return walking_height(x,z,nullptr);},
                [&](double ax,double az,double bx,double bz,float y,float h,float radius){
                    if(!world_scenery_can_move(scenery,ax,az,bx,bz,y,h,radius))return false;
                    if(active_playtest_walk||active_city_physics){
                        ++source_camera_casts;
                        world_physics::Hit hit;const world_physics::Vec3 center{ax,y+h*.5,az},delta{bx-ax,0,bz-az};
                        const float half=world_follow_camera_capsule_half_height(h,radius);
                        const bool queried=active_playtest_walk?active_playtest_walk->cast_capsule(center,radius,half,delta,hit):active_city_physics->cast_capsule(center,radius,half,delta,hit);
                        if(!queried)++source_camera_query_failures;
                        if(!queried||(hit.hit&&hit.fraction<.999f)){++source_camera_blocks;return false;}
                    }
                    return true;
                },combat_shoulder_active?closeup.shoulder:exploration.shoulder);
            render_cam.x=shoulder.x;render_cam.z=shoulder.z;combat_shoulder_offset=shoulder.offset;
            // Validate the composed shoulder/boom, then the actual eye motion.
            // The two safe component sweeps alone can still peek through a jamb.
            const auto camera_segment_clear=[&](WorldCameraPoint a,WorldCameraPoint b,float radius){
                if(!world_road_traffic_camera_clear(road_traffic,a.x,a.y,a.z,b.x,b.y,b.z,radius))return false;
                const float low=float(std::min(a.y,b.y)-radius-.02),height=float(std::abs(a.y-b.y)+2*(radius+.02));
                if(!world_scenery_can_move(scenery,a.x,a.z,b.x,b.z,low,height,radius))return false;
                if(active_playtest_walk||active_city_physics){
                    ++source_camera_casts;world_physics::Hit hit;
                    const world_physics::Vec3 center{a.x,a.y,a.z},delta{b.x-a.x,b.y-a.y,b.z-a.z};
                    const bool queried=active_playtest_walk?active_playtest_walk->cast_capsule(center,radius,.02f,delta,hit):active_city_physics->cast_capsule(center,radius,.02f,delta,hit);
                    if(!queried)++source_camera_query_failures;
                    if(!queried||hit.hit){++source_camera_blocks;return false;}
                }
                if(!active_playtest_walk||active_structure_world){
                    const unsigned count=std::clamp(unsigned(std::ceil(world_camera_distance(a,b)/.15)),1u,64u);
                    for(unsigned i=0;i<=count;++i){const auto p=world_camera_mix(a,b,double(i)/count);
                        if(p.y<walking_height(p.x,p.z,nullptr)+radius+.02)return false;}
                }
                return true;
            };
            const float lens_radius=world_follow_camera_lens_radius(view_near_plane(),std::max(render_cam.fov,1.24f),render_cam.aspect);
            follow_lens_radius=lens_radius;follow_pivot_y=follow.pivot_y;
            const auto safe_eye=follow_camera_safety.update({cam.x,follow.pivot_y,cam.z},{render_cam.x,render_cam.y,render_cam.z},lens_radius,dt,
                temporal_cut||opening_camera_locked||studio.open,camera_segment_clear,follow.clear_distance<exploration.boom-.0001f||shoulder.blocked);
            camera_final_line_blocks+=safe_eye.line_blocked;camera_eye_motion_blocks+=safe_eye.motion_blocked;camera_final_occluded_frames+=!safe_eye.view_clear;
            follow_line_blocked=safe_eye.line_blocked;follow_motion_blocked=safe_eye.motion_blocked;follow_view_clear=safe_eye.view_clear;follow_recovering=safe_eye.recovering;
            render_cam.x=safe_eye.eye.x;render_cam.y=safe_eye.eye.y;render_cam.z=safe_eye.eye.z;
            // Retraction owns the next frame's extension, preventing a full
            // distance pop immediately after the corner becomes clear.
            if(safe_eye.line_blocked||safe_eye.motion_blocked||safe_eye.recovering){
                // The boom parameter is not Euclidean distance: its small
                // vertical rise changes the length at nonzero camera pitch.
                // Project onto that exact ray, and feed back only a correction.
                const double bx=-std::sin(cam.yaw)*std::cos(cam.pitch),by=-std::sin(cam.pitch)+.35/world_follow_camera_max_distance,bz=std::cos(cam.yaw)*std::cos(cam.pitch);
                const double parameter=((safe_eye.eye.x-cam.x)*bx+(safe_eye.eye.y-follow.pivot_y)*by+(safe_eye.eye.z-cam.z)*bz)/(bx*bx+by*by+bz*bz);
                boom_distance=float(std::clamp(parameter,0.,double(follow.distance)));combat_shoulder_offset=0;
            }
            // Visibility follows the actual corrected eye, not a requested
            // boom already contracted by an earlier component sweep.
            follow_distance=float(world_camera_distance({cam.x,follow.pivot_y,cam.z},safe_eye.eye));
            combat_shoulder_peak=std::max(combat_shoulder_peak,shoulder.offset);combat_shoulder_blocks+=shoulder.blocked;
            combat_shoulder_safe=std::isfinite(shoulder.offset)&&shoulder.offset>=0&&shoulder.offset<=world_combat_shoulder_maximum+.00001f;
            if(combat_shoulder_active){
                ++combat_shoulder_frames;
                combat_shoulder_target_x=closeup_target_x;combat_shoulder_target_y=preview_arena_feet+1.28;combat_shoulder_target_z=closeup_target_z;
                // An explicit arena focus owns the framing target. The HUD's
                // changing nearest opponent can never steal the camera.
                if(arena_enabled&&arena_target_lock.locked()){const auto &target=arena_game.challengers()[arena_target_lock.target()];combat_shoulder_target_x=arena_origin_x+target.position.x;combat_shoulder_target_y=arena_floor+target.position.y+1.28;combat_shoulder_target_z=arena_origin_z+target.position.z;}
                else if(arena_enabled){
                    // Free melee view follows the player's own aim line at
                    // weapon range. It never acquires or tracks an opponent.
                    constexpr double range=2.;const auto aim=cam.forward();
                    combat_shoulder_target_x=cam.x+aim.x*range;combat_shoulder_target_y=cam.y+aim.y*range;combat_shoulder_target_z=cam.z+aim.z*range;
                }
                combat_shoulder_target_y-=closeup.target_drop;
                const double dx=combat_shoulder_target_x-render_cam.x,dz=combat_shoulder_target_z-render_cam.z;
                render_cam.yaw=float(std::atan2(dx,-dz));render_cam.pitch=float(std::atan2(combat_shoulder_target_y-render_cam.y,std::hypot(dx,dz)));
            }
        }else{follow_camera_safety.reset();character_step_height.valid=false;boom_distance=world_follow_camera_max_distance;combat_shoulder_offset=0;combat_shoulder_active=false;combat_shoulder_safe=true;combat_closeup_camera.reset();exploration_camera.reset();}
        // Blend orientation only after the unmodified collision-swept boom and
        // shoulder have been placed. The exact logical camera still owns aim,
        // locomotion, attacks, first-person arms and the target-lock controller.
        combat_focus_goal_yaw=render_cam.yaw;combat_focus_goal_pitch=render_cam.pitch;
        const bool focus_camera_mode=third_person&&!cam.fly&&!studio.open&&!preview_line&&!opening_camera_locked&&
            !milestone_review.active()&&!source_review.active&&!destination_loading&&!(arena_enabled&&arena_look_held());
        const unsigned focus_camera_context=(third_person?1u:0u)|(cam.fly?2u:0u)|(cam.swimming?4u:0u)|
            (cam.dive.diving?8u:0u)|(arena_bow()?16u:0u)|(focus_camera_mode?32u:0u);
        WorldCombatFocusFraming focus_player_framing;
        for(unsigned i=0;i<2;++i){
            const double dx=cam.x-render_cam.x,dz=cam.z-render_cam.z;
            focus_player_framing.player[i]={std::atan2(dx,-dz),
                std::atan2(cam.y-1.75+(i?1.8:.04)-render_cam.y,std::hypot(dx,dz))};
        }
        combat_focus_frame=combat_focus_camera.update(combat_focus_goal_yaw,combat_focus_goal_pitch,dt,
            focus_camera_mode&&combat_shoulder_active,arena_enabled&&arena_target_lock.locked()?arena_target_lock.target()+1:0,
            focus_camera_context,temporal_cut||!focus_camera_mode||fight_paused,render_cam.fov,render_cam.aspect,
            focus_camera_mode?&focus_player_framing:nullptr);
        combat_focus_applied=focus_camera_mode;
        if(combat_focus_applied){render_cam.yaw=float(combat_focus_frame.yaw);render_cam.pitch=float(combat_focus_frame.pitch);}
        if(qa_grass&&grass_qa_phase>0){
            if(!grass_qa_observer_ready){grass_qa_observer=render_cam;grass_qa_observer.pitch=-.45f;grass_qa_observer_ready=true;temporal_cut=true;}
            render_cam=grass_qa_observer;render_cam.aspect=cam.aspect;
        }
        portrait_focus_distance=0;
        if(opening_enabled)opening_render_camera(render_cam,dt);
        cutscene_render_camera(render_cam);
        joust_camera(render_cam);
        const bool opening_dialogue_visual=opening_enabled&&(opening_frame.camp_dialogue||opening_dialogue_blend.conversation_transitioning());
        if(milestone_review.portrait()) {
            const auto p=world_milestone::portrait(cam.x,cam.y-1.75,cam.z,cam.yaw,milestone_review);
            render_cam=cam;render_cam.x=p.x;render_cam.y=p.y;render_cam.z=p.z;render_cam.yaw=p.yaw;render_cam.pitch=p.pitch;render_cam.fov=p.fov;
            portrait_focus_distance=p.focus;follow_distance=p.focus;milestone_review.portrait_frames++;
        }
        if(!milestone_review.portrait()&&opening_enabled&&!studio.open&&graphics.environment_effects!=WorldEnvironmentEffects::Low){
            int focus_actor=opening_frame.npc_intro?opening_frame.npc_intro_speaker_actor:opening_frame.camp_dialogue?int(opening_frame.camp_actor):-1;
            if(first_journey_enabled&&first_journey_frame.dialogue&&first_journey_frame.dialogue_actor<3)focus_actor=int(first_journey_frame.dialogue_actor);
            WorldTrailCamper speaker;
            if(focus_actor>=0&&focus_actor<3&&world_trail_camp_actor(arrival_camp,unsigned(focus_actor),&speaker)){
                const double dx=speaker.x-render_cam.x,dy=speaker.y+1.48-render_cam.y,dz=speaker.z-render_cam.z;
                const double distance=std::sqrt(dx*dx+dy*dy+dz*dz);
                if(distance>1.1&&distance<8.)portrait_focus_distance=float(distance);
            }
        }
        water_post_hands=false;
        {float surface=0;const bool wet=!studio.open&&!cam.fly&&world_atlas_local_water(render_cam.x,render_cam.z,&surface,nullptr);
         water_post_depth=wet?std::max(0.f,surface-float(render_cam.y)):0.f;
         water_post_blend=std::clamp((water_post_depth-.025f)/.15f,0.f,1.f);water_post_time=visual_time;}
        arena_prepare_render(render_cam,fight_paused?0.f:dt,temporal_cut);
        dressing.update(render_cam,launch->frames?1./60.:double(dt),destination_loading);
        outcrops.update(render_cam);
        horizon.update(render_cam);
        const bool draw_avatar=!joust_enabled&&third_person&&!cam.fly&&follow_distance>.75f&&(!presentation||preview_screen!=0)&&!opening_dialogue_visual&&!(opening_enabled&&opening_frame.cinematic);
        if(third_person_observation)third_person_observation<<"{\"frame\":"<<frame<<",\"third_person\":"<<(third_person?"true":"false")
            <<",\"follow_m\":"<<follow_distance<<",\"draw_avatar\":"<<(draw_avatar?"true":"false")
            <<",\"focus\":"<<(arena_target_lock.locked()?int(arena_target_lock.target()):-1)<<",\"paused\":"<<(fight_paused?"true":"false")
            <<",\"health\":"<<fight.health<<",\"attack_serial\":"<<fight.attack_start_serial<<",\"weapon\":"<<unsigned(fight.weapon)
            <<",\"guard\":"<<(fight.guard?"true":"false")<<",\"dodge\":"<<fight.dodge_time
            <<",\"eye\":["<<render_cam.x<<','<<render_cam.y<<','<<render_cam.z<<','<<render_cam.yaw<<','<<render_cam.pitch<<','<<render_cam.fov<<']'
            <<",\"root\":["<<cam.x<<','<<cam.y-1.75<<','<<cam.z<<"],\"framing_clamps\":"<<combat_focus_camera.stats().player_framing_clamps
            <<",\"camera_line_blocks\":"<<camera_final_line_blocks<<",\"camera_motion_blocks\":"<<camera_eye_motion_blocks<<",\"camera_occluded_frames\":"<<camera_final_occluded_frames
            <<",\"camera_pivot\":["<<cam.x<<','<<follow_pivot_y<<','<<cam.z<<"],\"camera_radius\":"<<follow_lens_radius
            <<",\"camera_line_blocked\":"<<(follow_line_blocked?"true":"false")<<",\"camera_motion_blocked\":"<<(follow_motion_blocked?"true":"false")
            <<",\"camera_view_clear\":"<<(follow_view_clear?"true":"false")<<",\"camera_recovering\":"<<(follow_recovering?"true":"false")<<",\"camera_cut\":"<<(temporal_cut?"true":"false")<<",\"camera_safety_playable\":"<<(third_person&&!cam.fly&&!opening_camera_locked&&!opening_dialogue_blend.transitioning()&&!studio.open&&!destination_loading&&preview_screen==1?"true":"false")<<"}\n";
        const double skeletal_dx=avatar_initialized?cam.x-avatar_last_x:0.,skeletal_dz=avatar_initialized?cam.z-avatar_last_z:0.;
        const double travel=avatar_initialized?std::hypot(skeletal_dx,skeletal_dz):0.;
        const bool avatar_walking=travel>.0005&&travel<1.;
        const bool skeletal_hero_frame_context_ready=skeletal_hero_context_ready_now();
        if(!avatar_initialized||travel>10)avatar_yaw=3.14159265359f-cam.yaw;
        else if(captured_walk_request.captured&&!fight.engaged()&&!skeletal_terminal.active())avatar_yaw=captured_walk_request.facing_yaw;
        else if(avatar_walking&&!fight.engaged()&&!skeletal_terminal.active()){
            const double desired=std::atan2(cam.x-avatar_last_x,cam.z-avatar_last_z);
            avatar_yaw=world_character_presentation::walking_yaw(avatar_yaw,desired,dt);
        }
        // Publish the contact frame at its real heading once; subsequent
        // hit-stop frames keep that same physical and rendered body heading.
        const std::uint64_t player_contact_after=std::uint64_t(fight.hits)+fight.guarded+fight.parries+fight.blocks+fight.enemy_hits;
        if(fight.engaged()&&(!player_hitstop||fight.impact_pause<=0||player_contact_after!=player_contact_before))avatar_yaw=3.14159265359f-cam.yaw;
        if(skeletal_terminal.active())avatar_yaw=skeletal_terminal.yaw();
        avatar_last_x=cam.x;avatar_last_z=cam.z;avatar_initialized=true;
        if(presentation){world_combat_motion_arena(combat_motion,preview_arena_x,preview_arena_z,preview_arena_feet);if(battlefield_combat_motion)world_combat_motion_arena(battlefield_combat_motion,preview_arena_x,preview_arena_z,preview_arena_feet);}
        if(arena_enabled)world_combat_motion_arena(arena_orc_motion,preview_arena_x,preview_arena_z,preview_arena_feet);
        if(arena_compact_roster_enabled)arena_each_appearance([&](auto *motion){world_combat_motion_arena(motion,preview_arena_x,preview_arena_z,preview_arena_feet);});
        auto motion_fight=fight;
        skeletal_terminal.apply(motion_fight);
        if(arena_enabled)for(unsigned i=0;i<arena_game.events().count;++i)if(arena_game.events().values[i].kind==TrainingContactKind::EnemyBody)motion_fight.enemy_direction=arena_game.events().values[i].direction;
        // Victory keeps gameplay stopped while finishing loose-piece visuals.
        // Normal Fight motion owns its own single update; genuine pause/menu,
        // focus loss and the final impact hold still freeze these visual steps.
        if(arena_enabled&&fight_paused&&arena_ui_phase==ArenaUiPhase::Results&&arena_game.state()==world_arena_combat::State::Won&&
           !arena_results_paused()&&fight.impact_pause<=0)
        {if(arena_compact_roster_enabled)arena_each_appearance([&](auto *motion){world_combat_motion_settle_pieces(motion,arena_result_visual_dt);});
         else world_combat_motion_settle_pieces(arena_opponent_motion(),arena_result_visual_dt);}
        const float player_animation_dt=launch->frames&&!qa_walking_controls?1.f/60.f:dt;
        if(arena_enabled){
            world_combat_motion_update_arena(combat_motion,motion_fight,cam.x,cam.y-1.75,cam.z,avatar_yaw,player_animation_dt,nullptr,0,fight_paused);
            if(arena_compact_roster_enabled)arena_each_appearance([&](auto *motion){
                world_combat_motion_update_arena(motion,motion_fight,cam.x,cam.y-1.75,cam.z,avatar_yaw,player_animation_dt,
                    arena_motion_actors.data(),unsigned(arena_motion_actors.size()),fight_paused);
            });
            else if(arena_opponent_motion()!=combat_motion)world_combat_motion_update_arena(arena_orc_motion,motion_fight,cam.x,cam.y-1.75,cam.z,avatar_yaw,player_animation_dt,
                arena_motion_actors.data(),unsigned(arena_motion_actors.size()),fight_paused);
        }
        else world_combat_motion_update(combat_motion,motion_fight,cam.x,cam.y-1.75,cam.z,avatar_yaw,player_animation_dt,
            first_journey_enabled?first_journey_enemy_visible():arena_enabled?(arena_game.state()==world_arena_combat::State::Entering||arena_game.state()==world_arena_combat::State::Fighting||arena_game.state()==world_arena_combat::State::Lost):playtest_enabled?playtest_combat_visible():presentation?(fight.engaged()||fight_result_in_lane):(owned_enemy_near||fight.engaged()||fight_result_in_lane),fight_paused);
        if(battlefield_combat_motion)world_combat_motion_update(battlefield_combat_motion,motion_fight,cam.x,cam.y-1.75,cam.z,avatar_yaw,player_animation_dt,first_journey_enemy_visible(),fight_paused);
        float player_water_surface=0;const bool player_water=world_atlas_local_water(cam.x,cam.z,&player_water_surface,nullptr);
        const bool player_submerged=cam.swimming&&player_water&&world_swimming::submerged(cam.y,player_water_surface);
        world_combat_motion_unarmed(combat_motion,weapon_sheathed,fight_paused?0.f:dt,float(avatar_walking?travel:0));
        if(arena_player_bank)world_combat_motion_unarmed(arena_player_bank,weapon_sheathed,fight_paused?0.f:dt,float(avatar_walking?travel:0));
        skeletal_hero_active=false;skeletal_hero_draw=false;
        const bool skeletal_hero_supported=skeletal_hero_supported_now();
        if(skeletal_hero_loaded&&WorldSkeletalHeroLoadouts::supports_weapon(motion_fight.weapon)){
            WorldSkeletalHero::Input input;input.x=cam.x;input.y=cam.y-1.75;input.z=cam.z;input.dx=skeletal_dx;input.dz=skeletal_dz;
            input.dt=player_animation_dt;input.yaw=avatar_yaw;input.equipment_stowed=skeletal_equipment_stowed();
            input.swimming=story_native_swim();input.submerged=input.swimming&&player_submerged;input.dead=story_player_dead();
            input.paused=input.dead?(authored_cutscene_locked||cutscene_return_neutral||preview_screen!=1||show_map||show_graphics||inventory_open||
                studio.open||studio.production_freeze()||studio.cinematic_preview||destination_loading||
                !(SDL_GetWindowFlags(window)&SDL_WINDOW_INPUT_FOCUS)):
                ((skeletal_terminal.active()?skeletal_visual_paused:fight_paused)||studio.open||destination_loading);
            input.grounded=cam.grounded&&!cam.fly&&!cam.swimming;
            input.ground_query=skeletal_foot_support;
            input.reset=skeletal_hero_root_reset_pending()||skeletal_hero_supported!=skeletal_hero_previous_support;
            input.reaction_serial=skeletal_hero_hit_serial;input.reaction_direction=unsigned(motion_fight.enemy_direction);
            if(skeletal_guard_target)skeletal_guard_target(input);
            if(!skeletal_hero.update_cpu(input,motion_fight,skeletal_hero_error)||
               !skeletal_hero.prepare(live_programs.skeletal.id,live_programs.skeletal_shadow.id,world_program_epoch,skeletal_hero_error)||
               !skeletal_hero.prepare_first_person(skeletal_first_person.id,world_program_epoch,skeletal_hero_error)||
               !skeletal_hero.upload(skeletal_hero_error)){
                std::fprintf(stderr,"[Skeletal hero candidate] %s: %s\n",story_civilian_ready?"Story body update failed":"Safe legacy fallback",skeletal_hero_error.c_str());
                skeletal_hero_loaded=false;skeletal_hero_root_ready=false;skeletal_hero.release();
                if(story_civilian_ready){launch->exit_code=1;running=false;}
             }else{skeletal_hero_active=skeletal_hero_supported&&skeletal_hero_frame_context_ready;skeletal_hero_draw=skeletal_hero_active&&draw_avatar;skeletal_contact_pose_clock=fight.clock;skeletal_sweep_cache.count=0;incoming_pose_cache.ready=false;
                const auto &face_asset=skeletal_hero.active().asset();const auto &face_pose=skeletal_hero.active().pose();
                story_facial_targets=unsigned(face_asset.morph_targets.size());bool expression=false,blink=false;
                for(unsigned k=0;k<face_asset.morph_targets.size();++k){
                    const float weight=face_pose.morph_weights[k];expression=expression||weight>.0001f;
                    if(face_asset.morph_targets[k].name=="EyeBlinkLeft"||face_asset.morph_targets[k].name=="eyeBlinkRight"){
                        story_blink_peak=std::max(story_blink_peak,weight);blink=blink||weight>.05f;
                    }
                }
                story_facial_frames+=expression;story_blink_frames+=blink;
            }
            blade_clash_qa_input(input);
        }
        equipment_change.pose_committed();
        skeletal_hero_previous_support=skeletal_hero_supported;
        if(skeletal_hero_loaded){skeletal_hero_previous_root={cam.x,cam.y-1.75,cam.z};skeletal_hero_root_ready=true;}
        if(weapon_sheathed||equipment_change.active()||(skeletal_hero_loaded&&skeletal_hero.supports_equipment()&&!skeletal_hero.equipment_combat_ready())){fight.set_guard(false);fight.clear_buffer();}
        if(battlefield_enabled){
            const bool battlefield_paused=preview_screen!=1||show_graphics||destination_loading||!preview_pad.focused||studio.open;
            battlefield_visible=battlefield_state.stage()!=BattlefieldStage::Inactive&&(battlefield_frame.active||std::hypot(cam.x-world_battlefield_scene::origin_x,cam.z-world_battlefield_scene::origin_z)<85.);
            battlefield_quality=unsigned(graphics.environment_effects);
            const double visual_delta=battlefield_paused?0.:double(launch->frames?1.f/60.f:dt);
            if(battlefield_frame.events&world_battlefield_prologue::Started)battlefield_scene_seconds=0;
            if(battlefield_visible)battlefield_scene_seconds+=std::min(visual_delta,.25);
            if(!battlefield_update_visuals(visual_delta)){std::fprintf(stderr,"[Battlefield] Visuals: %s\n",battlefield_render_error.c_str());launch->exit_code=1;running=false;}
            world_battlefield_audio::Frame battlefield_sound;
            battlefield_sound.dt=std::min(visual_delta,.25);battlefield_sound.seconds=battlefield_scene_seconds;
            battlefield_sound.visible=battlefield_visible&&std::hypot(cam.x-world_battlefield_scene::origin_x,cam.z-world_battlefield_scene::origin_z)<220.;
            battlefield_sound.paused=battlefield_paused;battlefield_sound.focused=preview_pad.focused;battlefield_sound.loading=destination_loading;
            battlefield_sound.listener[0]=cam.x;battlefield_sound.listener[1]=cam.y;battlefield_sound.listener[2]=cam.z;battlefield_sound.yaw=cam.yaw;
            int bf_master=100,bf_music=100,bf_sfx=100;audio_get_volumes(&bf_master,&bf_music,&bf_sfx);battlefield_sound.volume=std::clamp(float(bf_master)*float(bf_sfx)/10000.f,0.f,1.f)*float(1.-battlefield_frame.muffle*.55);
            if(battlefield_audio_ready)battlefield_audio.update(battlefield_sound);
            const world_battlefield_audio::Point battlefield_contact{battlefield_config.ambush.feet.x,battlefield_config.ambush.feet.y+1.,battlefield_config.ambush.feet.z};
            if(battlefield_frame.events&world_battlefield_prologue::AmbushStarted)battlefield_audio.event(world_battlefield_audio::Cue::Ambush,battlefield_contact);
            if(battlefield_frame.events&world_battlefield_prologue::TackleImpact)battlefield_audio.event(world_battlefield_audio::Cue::Body,battlefield_contact);
            if(battlefield_frame.events&world_battlefield_prologue::StabContact)battlefield_audio.event(world_battlefield_audio::Cue::Stab,battlefield_contact);
            if(battlefield_frame.events&world_battlefield_prologue::Died)battlefield_audio.event(world_battlefield_audio::Cue::Death,{cam.x,cam.y,cam.z});
            if(battlefield_trace)battlefield_trace<<std::setprecision(12)<<"{\"frame\":"<<frame<<",\"quality_dt\":"<<quality_dt<<",\"render_size\":["<<target.w<<','<<target.h<<"],\"yaw\":"<<cam.yaw<<",\"enemy_root\":["<<battlefield_enemy_x<<','<<battlefield_enemy_y<<','<<battlefield_enemy_z<<"],\"struggle_presses\":"<<battlefield_frame.struggle_presses<<",\"struggle_required\":"<<battlefield_frame.struggle_required<<",\"stage\":"<<world_json::quote(world_battlefield_prologue::stage_name(battlefield_frame.stage))<<",\"elapsed\":"<<battlefield_frame.elapsed<<",\"events\":"<<battlefield_frame.events<<",\"dt\":"<<dt<<",\"loading\":"<<(destination_loading?"true":"false")<<",\"paused\":"<<(battlefield_paused?"true":"false")<<",\"screen\":"<<preview_screen<<",\"root\":["<<cam.x<<','<<cam.y-1.75<<','<<cam.z<<"],\"wins\":"<<battlefield_wins<<",\"health\":"<<fight.health<<",\"enemy_health\":"<<fight.enemy_health<<",\"enemy\":"<<battlefield_enemy_index<<",\"sword\":"<<(battlefield_state.sword_owned()?"true":"false")<<",\"actors\":"<<battlefield_scene_frame.actor_count<<",\"failures\":"<<battlefield_actors.stats().failures<<"}\n"<<std::flush;
            if(battlefield_stab_blood_pending&&battlefield_render_uploaded){
                const auto &a=battlefield_actors.anchors(WorldBattlefieldActors::Role::Attacker);
                if(a.visible&&battlefield_blood_queue.size()<8){const auto&p=a.chest;const double dx=std::sin(battlefield_config.ambush.yaw),dz=std::cos(battlefield_config.ambush.yaw);
                    battlefield_blood_queue.push_back({{p[0],p[1],p[2]},{dx,.16,dz},world_atlas_walk_height(p[0],p[2])+.02,true,1.5f,false});}
                battlefield_stab_blood_pending=false;
            }

        }
        if(aquatic_scene)world_combat_motion_aquatics(combat_motion,cam.swimming&&!cam.fly,player_submerged,avatar_walking,dt,fight_paused||studio.open||destination_loading);
        if(aquatic_scene){world_aquatic_effects::Input input;input.dt=studio.open||destination_loading?0:dt;input.enabled=!studio.open&&!cam.fly&&!destination_loading;
            input.player[0]=cam.x;input.player[1]=cam.y;input.player[2]=cam.z;input.observer[0]=render_cam.x;input.observer[1]=render_cam.y;input.observer[2]=render_cam.z;
            input.yaw=cam.yaw;input.swimming=cam.swimming;input.submerged=player_submerged;input.moving=avatar_walking;aquatic_system.update(input,aquatic_habitat);}
        {
            world_combat_vfx::Input input;input.first_person=!third_person;input.paused=fight_paused||studio.open;
            bool physical_blood_arrow=false;
            input.feet[0]={cam.x,cam.y-1.75,cam.z};input.feet[1]={fight_ox+fight.enemy_x,presentation?preview_arena_feet:24.,fight_oz+fight.enemy_z};
            const auto r=render_cam.right(),f=render_cam.forward(),u=norm(cross(r,f));
            for(unsigned actor=0;actor<2;++actor){WorldCombatBlade blade;if((actor||!arena_bow())&&world_combat_motion_blade(actor?arena_opponent_motion():arena_player_motion(),actor&&arena_enabled?unsigned(arena_game.current_index())+1:actor,blade)){input.blades[actor]={{blade.base[0],blade.base[1],blade.base[2]},{blade.tip[0],blade.tip[1],blade.tip[2]},true};}}
            if(skeletal_hero_active){input.blades[0]={};double base[3],tip[3];if(skeletal_hero.blade(base,tip))input.blades[0]={{base[0],base[1],base[2]},{tip[0],tip[1],tip[2]},true};}
            if(input.first_person&&skeletal_hero_active&&input.blades[0].valid){
                // Trails and camera-local arms share one presentation transform.
                // The unshifted blade remains authoritative for world contacts.
                auto shown=input.blades[0];
                const world_combat_vfx::Point root{cam.x,cam.y-1.75,cam.z};
                shown.base=world_skeletal_hero_viewmodel::world_trail_point(shown.base,root,cam.yaw);
                shown.tip=world_skeletal_hero_viewmodel::world_trail_point(shown.tip,root,cam.yaw);
                input.trail_blades[0]=shown;
            }
            if(input.first_person&&!arena_bow()&&!skeletal_hero_active){WorldCombatBlade blade;if(world_combat_motion_blade(combat_motion,0,blade,true)){const double lens=std::tan(render_cam.fov*.5)/std::tan(1.08*.5);auto point=[&](const double *p){return world_combat_vfx::Point{render_cam.x+r.x*p[0]*lens+u.x*p[1]*lens-f.x*p[2],render_cam.y+r.y*p[0]*lens+u.y*p[1]*lens-f.y*p[2],render_cam.z+r.z*p[0]*lens+u.z*p[1]*lens-f.z*p[2]};};input.trail_blades[0]={point(blade.base),point(blade.tip),true};}}
            if(arena_enabled&&arena_game.arrow_hits()>combat_vfx_arrow_hits){
                const world_arena_combat::Arrow *impact=nullptr;
                for(const auto &arrow:arena_game.arrows())if(arrow.state==world_arena_combat::ArrowState::HitEnemy&&(!impact||arrow.age<impact->age))impact=&arrow;
                if(impact){input.body_contacts[1]={{arena_origin_x+impact->position.x,arena_floor+impact->position.y,arena_origin_z+impact->position.z},true};physical_blood_arrow=true;}
            }
            if(arena_enabled)combat_vfx_arrow_hits=arena_game.arrow_hits();
            for(unsigned defender=0;defender<2;++defender){
                const auto from=input.feet[defender],to=input.feet[1-defender];
                const double distance=std::max(.001,std::hypot(to.x-from.x,to.z-from.z));
                const world_combat_vfx::Point outward{(to.x-from.x)/distance,0,(to.z-from.z)/distance};
                const world_combat_vfx::Point side{outward.z,0,-outward.x};
                const auto angle=defender?fight.attack_direction:fight.enemy_direction;
                const unsigned event=defender?fight.hits:fight.enemy_hits;
                const double sign=angle==TrainingDirection::Left?1.:angle==TrainingDirection::Right?-1.:(event&1)?1.:-1.;
                const bool arrow=defender&&physical_blood_arrow;
                const double front=arrow?.85:.35,lateral=arrow?.50:.94;
                input.body_directions[defender]={outward.x*front+side.x*lateral*sign,angle==TrainingDirection::High?.16:.08,outward.z*front+side.z*lateral*sign};
                const float damage=std::max(0.f,defender?fight_old_enemy_health-fight.enemy_health:fight_old_health-fight.health);
                const bool new_hit=defender?fight.hits>fight_old_hits:fight.enemy_hits>fight_old_enemy_hits;
                input.body_finisher[defender]=new_hit&&(defender?fight.enemy_health:fight.health)<=0;
                input.body_strength[defender]=input.body_finisher[defender]?1.5f:std::clamp(.85f+damage/90.f,.9f,1.25f);
            }
            if(!arena_enabled&&fight.hits>fight_old_hits){
                const auto &hit=combat_anatomical_hits[0];
                if(combat_anatomy_ready&&hit.valid&&!hit.weapon_contact&&combat_anatomical_serials[0]==fight.attack_start_serial)
                    input.body_contacts[1]={{hit.point[0],hit.point[1],hit.point[2]},true};
                else if(fight.enemy_decapitated){double neck[3];if(world_combat_motion_neck(arena_opponent_motion(),1,neck))input.body_contacts[1]={{neck[0],neck[1],neck[2]},true};}
                if(fight.enemy_severed_parts!=fight_old_severed_parts)input.body_strength[1]=1.85f;
            }
            input.attack_serial=fight.attack_start_serial;
            std::array<world_blood_effects::ResolvedEvent,64> arena_body_events={};unsigned arena_body_count=0;
            {
                input.resolved_events=true;
                input.actor_trails[0]={input.trail_blades[0].valid?input.trail_blades[0]:input.blades[0],
                    !arena_bow()&&fight.engaged()&&(fight.player_phase==TrainingPlayerPhase::Strike||(fight.player_phase==TrainingPlayerPhase::Windup&&fight.player_time>fight.player_duration*.62)),fight.attack_start_serial};
                for(unsigned id=0;arena_enabled&&id<arena_motion_actors.size();++id){
                    const auto &a=arena_motion_actors[id];const auto &v=a.combat;WorldCombatBlade blade;
                    if(a.visible&&world_combat_motion_blade(arena_opponent_motion(id),id+1,blade))input.actor_trails[id+1]={{{blade.base[0],blade.base[1],blade.base[2]},{blade.tip[0],blade.tip[1],blade.tip[2]},true},
                        v.engaged()&&(v.enemy_phase==TrainingEnemyPhase::Strike||(v.enemy_phase==TrainingEnemyPhase::Windup&&v.enemy_time>v.enemy_duration*.78)),v.enemy_swings};
                }
                if(!arena_enabled){WorldCombatBlade blade;
                    if(world_combat_motion_blade(arena_opponent_motion(),1,blade))input.actor_trails[1]={{{blade.base[0],blade.base[1],blade.base[2]},{blade.tip[0],blade.tip[1],blade.tip[2]},true},
                        fight.engaged()&&(fight.enemy_phase==TrainingEnemyPhase::Strike||(fight.enemy_phase==TrainingEnemyPhase::Windup&&fight.enemy_time>fight.enemy_duration*.78)),fight.enemy_swings};
                }
                const auto &events=arena_enabled?arena_game.events():ordinary_contact_events;
                for(unsigned n=0;n<events.count;++n){
                    const auto &e=events.values[n];if(e.opponent_id>=(arena_enabled?unsigned(arena_game.challengers().size()):1u))continue;
                    const bool outgoing=e.kind==TrainingContactKind::PlayerBody,incoming=e.kind==TrainingContactKind::EnemyBody;
                    const bool parry=e.kind==TrainingContactKind::PlayerParry,block=e.kind==TrainingContactKind::PlayerBlock,clash=e.kind==TrainingContactKind::EnemyBlock;
                    if(!outgoing&&!incoming&&!parry&&!block&&!clash)continue;
                    world_combat_vfx::Point enemy_feet=input.feet[1];
                    if(arena_enabled){const auto &actor=arena_game.challengers()[e.opponent_id];enemy_feet={arena_origin_x+actor.position.x,arena_floor,arena_origin_z+actor.position.z};}
                    const bool defending_player=incoming||parry||block;
                    const auto defender=defending_player?input.feet[0]:enemy_feet,attacker=defending_player?enemy_feet:input.feet[0];
                    const double distance=std::max(.001,std::hypot(attacker.x-defender.x,attacker.z-defender.z));
                    const world_combat_vfx::Point outward{(attacker.x-defender.x)/distance,0,(attacker.z-defender.z)/distance},side{outward.z,0,-outward.x};
                    const double sign=e.direction==TrainingDirection::Left?1.:e.direction==TrainingDirection::Right?-1.:(e.serial&1)?1.:-1.;
                    world_combat_vfx::Point direction{outward.x*.35+side.x*.94*sign,e.direction==TrainingDirection::High?.16:.08,outward.z*.35+side.z*.94*sign};
                    world_combat_vfx::Point point{defender.x+outward.x*.34,defender.y+1.18,defender.z+outward.z*.34};
                    WorldCombatBlade enemy_blade;const bool has_enemy=world_combat_motion_blade(arena_opponent_motion(e.opponent_id),e.opponent_id+1,enemy_blade);
                    auto closest_point=[](world_combat_vfx::Point a,world_combat_vfx::Point b,world_combat_vfx::Point q){const double dx=b.x-a.x,dy=b.y-a.y,dz=b.z-a.z;const double t=std::clamp(((q.x-a.x)*dx+(q.y-a.y)*dy+(q.z-a.z)*dz)/std::max(1.e-8,dx*dx+dy*dy+dz*dz),0.,1.);return world_combat_vfx::Point{a.x+dx*t,a.y+dy*t,a.z+dz*t};};
                    if(outgoing||incoming){
                        world_combat_vfx::Blade weapon=input.blades[0];if(incoming&&has_enemy)weapon={{enemy_blade.base[0],enemy_blade.base[1],enemy_blade.base[2]},{enemy_blade.tip[0],enemy_blade.tip[1],enemy_blade.tip[2]},true};
                        if(weapon.valid){auto near=closest_point(weapon.base,weapon.tip,{defender.x,defender.y+1.25,defender.z});point.y=std::clamp(near.y,defender.y+.65,defender.y+1.48);}
                    }else if(has_enemy&&input.blades[0].valid){
                        const world_combat_vfx::Point a{enemy_blade.base[0],enemy_blade.base[1],enemy_blade.base[2]},b{enemy_blade.tip[0],enemy_blade.tip[1],enemy_blade.tip[2]};
                        const auto on_player=closest_point(input.blades[0].base,input.blades[0].tip,a),on_enemy=closest_point(a,b,on_player),again=closest_point(input.blades[0].base,input.blades[0].tip,on_enemy);
                        point={(again.x+on_enemy.x)*.5,(again.y+on_enemy.y)*.5,(again.z+on_enemy.z)*.5};
                    }else point={(attacker.x+defender.x)*.5,defender.y+1.25,(attacker.z+defender.z)*.5};
                    if(e.anatomical)point={e.point[0],e.point[1],e.point[2]};
                    else if(e.projectile){point={arena_origin_x+e.point[0],arena_floor+e.point[1],arena_origin_z+e.point[2]};direction={outward.x*.85+side.x*.5*sign,.08,outward.z*.85+side.z*.5*sign};}
                    // Contact carries a real point, not a normal. Steel uses a
                    // bounded attacker-to-defender spark direction; blood keeps
                    // its existing outward spray, including anatomical hits.
                    if(parry||block||clash)direction={-outward.x,.28,-outward.z};
                    const double length=std::max(.001,std::hypot(direction.x,direction.y,direction.z));direction={direction.x/length,direction.y/length,direction.z/length};
                    if(e.decapitated){if(!e.anatomical){double neck[3];if(world_combat_motion_neck(arena_opponent_motion(e.opponent_id),e.opponent_id+1,neck))point={neck[0],neck[1],neck[2]};}direction.y=.32;}
                    const bool finisher=e.finisher;
                    const float strength=e.severed_part!=TrainingBodyPart::None||e.decapitated?1.85f:finisher?1.5f:std::clamp(.85f+e.damage/90.f,.9f,1.25f);
                    const auto visual_serial=arena_enabled?++arena_visual_contact_serial:e.serial;
                    if(input.contact_count<input.contacts.size())input.contacts[input.contact_count++]={visual_serial,outgoing?0u:incoming?3u:parry?1u:block?2u:4u,point,defender,direction,strength,finisher};
                    if((outgoing||incoming)&&arena_body_count<arena_body_events.size()){
                        arena_body_events[arena_body_count++]={visual_serial,incoming?0u:1u,{point,direction,arena_enabled?arena_floor+.032:std::max(world_atlas_walk_height(point.x,point.z),0.f),true,strength,finisher}};
                    }
                }
            }
            if(arena_enabled&&!input.paused){
                const auto &events=arena_game.team_events();
                for(unsigned n=0;n<events.count;++n){const auto &e=events.values[n];
                    if(e.attacker_id>=world_combat_capacity::npcs||e.defender_id>=world_combat_capacity::npcs)continue;
                    const bool steel=e.kind==TrainingContactKind::EnemyBlock||e.kind==TrainingContactKind::PlayerBlock||e.kind==TrainingContactKind::PlayerParry;
                    const bool flesh=e.kind==TrainingContactKind::EnemyBody||e.kind==TrainingContactKind::PlayerBody;
                    if(!steel&&!flesh)continue;
                    const auto &a=arena_game.challengers()[e.attacker_id],&d=arena_game.challengers()[e.defender_id];
                    const double length=std::max(.001,std::hypot(d.position.x-a.position.x,d.position.z-a.position.z));
                    const world_combat_vfx::Point point{arena_origin_x+e.point[0],arena_floor+e.point[1],arena_origin_z+e.point[2]},
                        feet{arena_origin_x+d.position.x,arena_floor+d.position.y,arena_origin_z+d.position.z},
                        direction{(d.position.x-a.position.x)/length,.24,(d.position.z-a.position.z)/length};
                    const auto serial=++arena_visual_contact_serial;
                    const float strength=std::clamp(.85f+e.damage/90.f,.9f,1.25f);
                    if(input.contact_count<input.contacts.size())input.contacts[input.contact_count++]={serial,steel?4u:0u,point,feet,direction,strength,false,false};
                    if(flesh&&arena_body_count<arena_body_events.size())arena_body_events[arena_body_count++]={serial,1u,{point,direction,arena_floor+.032,true,strength,false}};
                }
            }
            combat_vfx.update(fight,input,launch->frames?1./60.:dt);
            auto anatomical_stump=[&](unsigned id,TrainingBodyPart part,double point[3],double axis[3]){
                if(!arena_opponent_anatomy_ready(id))return false;
                if(arena_enabled&&arena_game.challengers()[id].state==world_arena_combat::ChallengerState::Defeated){
                    const auto &actor=arena_game.challengers()[id];const auto *enemy=arena_game.opponent(id);
                    return enemy&&world_combat_motion_stump_at(arena_opponent_motion(id),part,
                        float(std::clamp(actor.defeat_age/world_combat_motion::death_duration,0.,1.)),
                        arena_origin_x+actor.position.x,arena_floor+actor.position.y,arena_origin_z+actor.position.z,
                        actor.defeat_yaw,enemy->weapon_hand,point,axis,id+1);
                }
                return world_combat_motion_stump(arena_opponent_motion(id),id+1,part,point,axis);
            };
            std::array<world_neck_blood::Pose,world_combat_capacity::npcs> neck_poses={};
            for(unsigned id=0;id<(arena_enabled?unsigned(arena_game.challengers().size()):1u);++id){
                auto &pose=neck_poses[id];const auto *enemy=arena_enabled?arena_game.opponent(id):nullptr;
                pose.severed=arena_enabled?(enemy&&enemy->decapitated):fight.enemy_decapitated;
                if(!pose.severed)continue;double point[3],axis[3];
                const float collapse=arena_enabled?float(arena_game.challengers()[id].defeat_age/world_combat_motion::death_duration):-1.f;
                const float yaw=arena_enabled?arena_game.challengers()[id].defeat_yaw:0.f;
                pose.valid=arena_opponent_anatomy_ready(id)?anatomical_stump(id,TrainingBodyPart::Head,point,axis):
                    world_combat_motion_neck_emitter(arena_opponent_motion(id),id+1,point,axis,collapse,yaw);
                if(pose.valid){pose.point={point[0],point[1],point[2]};pose.axis={axis[0],axis[1],axis[2]};pose.floor=arena_enabled?arena_floor+.032:std::max(world_atlas_walk_height(point[0],point[2]),0.f);}
            }
            neck_blood.update(neck_poses,launch->frames?1./60.:dt,fight.clock,input.paused||destination_loading);
            const auto neck_stage=neck_blood.stage();std::array<world_blood_effects::Contact,24> neck_fluids={};
            for(unsigned i=0;i<neck_stage.count;++i){const auto &spurt=neck_stage.pulses[i];
                combat_vfx.neck_spurt(spurt.point,spurt.axis,spurt.floor,spurt.strength,spurt.droplets);
                neck_fluids[i]={spurt.point,spurt.axis,spurt.floor,true,spurt.strength,true};
            }
            std::array<world_sever_blood::Pose,world_sever_blood::capacity> wound_poses={};
            for(unsigned id=0;id<(arena_enabled?unsigned(arena_game.challengers().size()):1u);++id){
                const auto *enemy=arena_enabled?arena_game.opponent(id):nullptr;
                const auto mask=arena_enabled?(enemy?enemy->severed_parts:0):fight.enemy_severed_parts;
                for(unsigned part_index=1;part_index<world_sever_blood::parts;++part_index){
                    const auto part=TrainingBodyPart(part_index+1);
                    if(!(mask&world_combat_anatomy::bit(part)))continue;
                    auto &pose=wound_poses[id*world_sever_blood::parts+part_index];pose.severed=true;
                    double point[3],axis[3];pose.valid=anatomical_stump(id,part,point,axis);
                    if(!pose.valid)continue;
                    pose.point={point[0],point[1],point[2]};pose.axis={axis[0],axis[1],axis[2]};
                    pose.floor=arena_enabled?arena_floor+.032:std::max(world_atlas_walk_height(point[0],point[2]),0.f);
                    pose.strength=part==TrainingBodyPart::LeftHand||part==TrainingBodyPart::RightHand?1.25f:part==TrainingBodyPart::Torso?1.9f:1.6f;
                }
            }
            sever_blood.update(wound_poses,launch->frames?1./60.:dt,fight.clock,input.paused||destination_loading,
                unsigned(neck_fluids.size())-neck_stage.count);
            const auto wound_stage=sever_blood.stage();
            for(unsigned i=0;i<wound_stage.count;++i){const auto &spurt=wound_stage.pulses[i];
                combat_vfx.neck_spurt(spurt.point,spurt.axis,spurt.floor,spurt.strength,spurt.droplets);
                neck_fluids[neck_stage.count+i]={spurt.point,spurt.axis,spurt.floor,true,spurt.strength,true};
            }
            if(blood_renderer){
                const auto contacts=combat_vfx.stats();world_blood_effects::Input body;
                body.outgoing_hits=fight.hits;body.incoming_hits=fight.enemy_hits;body.combat_clock=fight.clock;body.paused=input.paused;
                body.resolved_events=true;body.events=arena_body_events.data();body.event_count=arena_body_count;
                unsigned fluid_count=neck_stage.count+wound_stage.count;
                for(const auto &event:battlefield_blood_queue)if(fluid_count<neck_fluids.size())neck_fluids[fluid_count++]=event;
                body.visual_spurts=neck_fluids.data();body.visual_spurt_count=fluid_count;
                if(battlefield_frame.active)body.paused=preview_screen!=1||show_graphics||destination_loading||!preview_pad.focused;

                for(unsigned defender=0;defender<2;++defender){
                    // The dressed arena sand is .032m above its physics floor.
                    const auto point=contacts.last_body_contacts[defender];
                    const double ground=first_journey_enabled?std::max(world_atlas_walk_height(point.x,point.z),0.f):arena_floor+.032;
                    body.contacts[defender]={point,input.body_directions[defender],ground,true,input.body_strength[defender],input.body_finisher[defender]};
                }
                if(!body.paused)battlefield_blood_queue.clear();
                if(!blood_effects.update(body,launch->frames?1./60.:dt)){std::fprintf(stderr,"[Blood] Invalid body event input\n");launch->exit_code=1;running=false;}
            }
            if(presentation&&!preview_settings.reduced_motion&&!fight_paused){render_cam.pitch+=combat_feedback.camera_impulse();render_cam.yaw+=combat_feedback.camera_yaw();}
        }
        feedback_qa_observe(frame);
        blade_clash_qa_observe(frame);
        equipment_qa_observe(frame,player_controls_allowed,fight_paused);
        animation_polish_qa_observe(frame);
        auto draw_player=[&](unsigned program,const Camera &eye,bool shadow,bool show_player=true){
            if(joust_enabled){joust_renderer.draw(program,eye.x,eye.y,eye.z,shadow,third_person||joust_fall_view);if(!shadow){glUniform1f(glGetUniformLocation(program,"uActorReactive"),.85f);joust_splinters.draw(program);glUniform1f(glGetUniformLocation(program,"uActorReactive"),0);}return;}
            if(!first_journey_enabled||battlefield_state.sword_owned()||(first_journey_outdoors()&&story_sword_unlocked()))world_combat_motion_draw(combat_motion,program,eye.x,eye.y,eye.z,shadow,show_player&&!story_civilian_ready&&!skeletal_hero_active&&!cam.fly&&!arena_player_bank&&(!arena_bow()||weapon_sheathed));
            if(battlefield_combat_motion){world_combat_motion_draw(battlefield_combat_motion,program,eye.x,eye.y,eye.z,shadow,false);world_combat_motion_draw_crowd(battlefield_combat_motion,program,eye.x,eye.y,eye.z,shadow);}
            if(arena_compact_roster_enabled)arena_each_appearance([&](auto *motion){world_combat_motion_draw(motion,program,eye.x,eye.y,eye.z,shadow,show_player&&!skeletal_hero_active&&!cam.fly);});
            else if(arena_enabled&&arena_opponent_motion()!=combat_motion)world_combat_motion_draw(arena_opponent_motion(),program,eye.x,eye.y,eye.z,shadow,false);
            if(arena_enabled){world_arena_render_draw(arena_renderer,program,eye.x,eye.y,eye.z,shadow);if(arena_compact_roster_enabled)arena_each_appearance([&](auto *motion){world_combat_motion_draw_crowd(motion,program,eye.x,eye.y,eye.z,shadow);});else world_combat_motion_draw_crowd(arena_opponent_motion(),program,eye.x,eye.y,eye.z,shadow);world_arena_render_draw_arrows(arena_renderer,program,eye.x,eye.y,eye.z,shadow);if(arena_bow()&&!weapon_sheathed&&third_person){if(arena_player_bank)world_combat_motion_draw_bow_actor(arena_player_bank,program,eye.x,eye.y,eye.z,false,shadow);else world_arena_render_draw_bow_actor(arena_renderer,program,eye.x,eye.y,eye.z,false,shadow);world_arena_render_draw_bow(arena_renderer,program,eye.x,eye.y,eye.z,shadow);}}
            if(playtest_enabled&&combat_motion&&!shadow){if(show_player&&!cam.fly)++playtest_hero_draw_frames;if(playtest_combat_visible())++playtest_enemy_visible_frames;}
        };
        bool dragons_loading_frame=false;
        if(dragons_enabled){
            if(dragons_pending>=0){
                if(!commit_dragons(dragons_pending==1)){std::fprintf(stderr,"[World] %s\n",dragons_error);launch->exit_code=1;running=false;}
                dragons_pending=-1;dragons_loading_frame=true;temporal_cut=true;
            }else{
                const bool wanted=world_castle_dragons_desired(dragons,cam.x,cam.z);
                if(wanted!=world_castle_dragons_stats(dragons).active){dragons_pending=wanted?1:0;destination_loading=true;dragons_loading_frame=true;}
            }
            if(!update_dragon_emitters(visual_time)){dragon_breath_failures++;launch->exit_code=1;running=false;}
            dragon_peak_breathing=std::max(dragon_peak_breathing,world_castle_dragons_stats(dragons).breathing);
        }
        studio.sync_object_preview(scenery);
        world_scenery_update(scenery,render_cam.x,render_cam.y,render_cam.z,render_cam.yaw,render_cam.pitch,render_cam.aspect,std::tan(render_cam.fov*.5f),foliage_detail_scale,studio.open?1.f:foliage_density_scale);
        // Decode/upload only behind a held destination image, after the first
        // loading frame has already reached the display. Ready walking frames
        // only query the fixed CPU catalog; a return to town releases the bank.
        bool wildlife_loading_frame=false;
        if(wildlife_pending!=-2) {
            if(!world_wildlife_commit(wildlife,wildlife_pending,wildlife_error,sizeof(wildlife_error))) {
                std::fprintf(stderr,"[World] %s\n",wildlife_error);launch->exit_code=1;running=false;
            }
            wildlife_pending=-2;wildlife_loading_frame=true;temporal_cut=true;
        } else {
            // A destination can prepare its nearby bank within the existing
            // residency envelope before play resumes. Ordinary traversal keeps
            // the original enter/exit hysteresis and staged loading policy.
            const int wanted=desired_wildlife(destination_loading);
            if(wanted>=-1&&wanted!=world_wildlife_stats(wildlife).habitat) {
                wildlife_pending=wanted;destination_loading=true;wildlife_loading_frame=true;
            }
        }
        world_wildlife_set_camp_quality(wildlife,world_environment_effects_profile(graphics.environment_effects).ambient_particles,world_environment_effects_profile(graphics.environment_effects).life_distance);
        world_wildlife_update(wildlife,scenery,visual_time);
        bool bandit_loading_frame=false;
        if(bandit_pending!=-2) {
            if(!world_bandit_camp_commit(bandits,bandit_pending==0,bandit_error,sizeof(bandit_error))) {
                std::fprintf(stderr,"[World] Bandit camp: %s\n",bandit_error);launch->exit_code=1;running=false;
            }
            bandit_pending=-2;bandit_loading_frame=true;temporal_cut=true;
        }else{
            const int wanted=city_showcase?-1:world_bandit_camp_desired(bandits,cam.x,cam.z);
            if(wanted>=-1&&(wanted==0)!=world_bandit_camp_stats(bandits).active) {
                bandit_pending=wanted;destination_loading=true;bandit_loading_frame=true;
            }
        }
        world_bandit_camp_update(bandits,scenery,visual_time);
        bool trail_camp_loading_frame=false;
        if(trail_camp_pending>=0){
            if(!world_trail_camp_commit(trail_camp,trail_camp_pending==1,trail_camp_error,sizeof(trail_camp_error))){std::fprintf(stderr,"[World] %s\n",trail_camp_error);launch->exit_code=1;running=false;}
            trail_camp_pending=-1;trail_camp_loading_frame=true;temporal_cut=true;
        }else{
            const bool wanted=population_enabled&&world_trail_camp_desired(trail_camp,cam.x,cam.z);
            if(wanted!=world_trail_camp_stats(trail_camp).active){trail_camp_pending=wanted?1:0;destination_loading=true;trail_camp_loading_frame=true;}
        }
        world_trail_camp_update(trail_camp,scenery,visual_time);
        bool arrival_camp_loading_frame=false;
        if(arrival_camp_pending>=0){
            if(!world_trail_camp_commit(arrival_camp,arrival_camp_pending==1,arrival_camp_error,sizeof(arrival_camp_error))){std::fprintf(stderr,"[Opening] %s\n",arrival_camp_error);launch->exit_code=1;running=false;}
            arrival_camp_pending=-1;arrival_camp_loading_frame=true;temporal_cut=true;
        }else{
            const bool wanted=arrival_camp_enabled&&world_trail_camp_desired(arrival_camp,cam.x,cam.z);
            if(wanted!=world_trail_camp_stats(arrival_camp).active){arrival_camp_pending=wanted?1:0;destination_loading=true;arrival_camp_loading_frame=true;}
        }
        if(arrival_camp_enabled){
            // Social blends follow elapsed presentation time, even when a slow
            // frame caps the separate physics step. Clip sampling already uses
            // visual_time; do not make the reaction lag behind that clock.
            const float present_dt=qa_fixed_time?0.f:(launch->frames?1.f/60.f:std::clamp(float(quality_dt),0.f,.25f));
            const bool quest_dialogue=first_journey_enabled&&first_journey_frame.dialogue;
            const auto *sample=opening_frame.npc_intro&&npc_intro_qa&&npc_intro_qa_visual_sample?npc_intro_qa_visual_sample():nullptr;
            const int actor=sample?sample->speaker_actor:opening_frame.npc_intro?opening_frame.npc_intro_speaker_actor:quest_dialogue?first_journey_frame.dialogue_actor:opening_frame.camp_dialogue?opening_frame.camp_actor:-1;
            const char *speaker=sample?sample->speaker:opening_frame.npc_intro?opening_frame.npc_intro_speaker:quest_dialogue?first_journey_frame.speaker:opening_frame.camp_speaker;
            const bool speaking=speaker&&*speaker&&std::strcmp(speaker,"You")!=0;
            const unsigned beat=quest_dialogue?first_journey_frame.beat:opening_state.camp().beat();
            const Camera &listener=opening_frame.npc_intro?render_cam:cam;
            world_trail_camp_present(arrival_camp,actor>=0&&actor<3?actor:-1,speaking,beat,listener.x,listener.y,listener.z,present_dt);
            if(first_journey_enabled)world_trail_camp_present(first_journey_townsfolk,actor==3?0:-1,speaking,beat,cam.x,cam.y,cam.z,present_dt);
        }
        world_trail_camp_update(arrival_camp,scenery,visual_time);
        world_trail_camp_update(first_journey_townsfolk,scenery,visual_time);
        g_engine_perf.raw_update_ms = millis(frame_start);
        g_engine_perf.update_ms = g_engine_perf.raw_update_ms;
        Uint64 render_start = SDL_GetPerformanceCounter();
        peak_grass = std::max(peak_grass, dressing.grass.blades);
        peak_scanned_grass=std::max(peak_scanned_grass,dressing.scanned_grass_count);
        peak_trees = std::max(peak_trees, dressing.tree_count+horizon.drawn);
        peak_near=std::max(peak_near,dressing.near_tree_count);peak_mid=std::max(peak_mid,dressing.mid_tree_count);
        peak_local_cards=std::max(peak_local_cards,dressing.far_tree_count);peak_horizon=std::max(peak_horizon,horizon.drawn);
        peak_horizon_roots=std::max(peak_horizon_roots,horizon.live.size());
        V3 forward = render_cam.forward(), right = render_cam.right(), up = norm(cross(right, forward));
        if(cutscene_frame.active){
            const auto f=world_cinematics::forward(cutscene_frame.camera.rotation),u=world_cinematics::up(cutscene_frame.camera.rotation);
            forward={float(f.x),float(f.y),float(f.z)};up={float(u.x),float(u.y),float(u.z)};right=norm(cross(forward,up));
        }
        M4 view = look({0, 0, 0}, forward, cutscene_frame.active?up:V3{0,1,0});
        M4 projection=perspective(render_cam.fov,render_cam.aspect,view_near_plane(),15000.f);
        float jitter[2]={};
        if(temporal) {
            world_temporal_jitter(temporal_sample,jitter);
            projection.m[8]-=2*jitter[0]/target.w;
            projection.m[9]-=2*jitter[1]/target.h;
        }
        M4 vp=product(projection,view);
        const auto authored_window_flags=SDL_GetWindowFlags(window);
        const bool authored_focused=!(authored_window_flags&SDL_WINDOW_MINIMIZED)&&(launch->frames||launch->benchmark||(authored_window_flags&SDL_WINDOW_INPUT_FOCUS));
        const bool authored_paused=!studio.open&&(show_graphics||inventory_open||(presentation&&preview_screen!=1));
        const auto authored_listener=authored_weather_frame(studio.open?render_cam:cam,launch->frames?1.f/60.f:dt,authored_focused,authored_paused);
        const auto authored_view=authored_weather_at(render_cam.x,render_cam.z);
        const float rainforest_weight=world_atlas_rainforest_weight(render_cam.x,render_cam.z);
        const auto rainforest_atmosphere=world_rainforest_atmosphere(cloud,rainforest_weight,visual_surface_finish);
        Lighting light = local_lighting(hour,authored_view.cloud,visual_time,rainforest_weight,bearing,sun_strength,sky_fill);
        float climate_influence=active_authored_weather.global_authored?1.f:0.f;
        if(!active_authored_weather.global_authored){float uncovered=1;for(unsigned i=0;i<active_authored_weather.count;++i)uncovered*=1-world_weather_authoring::area_weight(active_authored_weather.areas[i],render_cam.x,render_cam.z);climate_influence=1-uncovered;}
        auto storm_climate=world_storm_lighting::climate(authored_view,light.sun.y,climate_influence>0);
        storm_climate.direct=1+(storm_climate.direct-1)*climate_influence;storm_climate.sky=1+(storm_climate.sky-1)*climate_influence;
        storm_climate.ground=1+(storm_climate.ground-1)*climate_influence;storm_climate.environment=1+(storm_climate.environment-1)*climate_influence;
        storm_climate.fog=1+(storm_climate.fog-1)*climate_influence;storm_climate.desaturate*=climate_influence;
        light.sunColor=mul(light.sunColor,storm_climate.direct);light.ambient=mul(light.ambient,storm_climate.sky);
        light.skyLight=mul(light.skyLight,storm_climate.sky);light.groundLight=mul(light.groundLight,storm_climate.ground);
        light.environment_gain*=storm_climate.environment;light.fog=mul(light.fog,storm_climate.fog);
        const float zenith_luma=dot(light.skyZenith,{.2126f,.7152f,.0722f});
        light.skyZenith=mul(add(mul(light.skyZenith,1-storm_climate.desaturate),mul(V3{zenith_luma,zenith_luma,zenith_luma},storm_climate.desaturate)),storm_climate.sky);
        active_storm_light=world_storm_lighting::flash(authored_storm_output,light.sun.y);

        WorldTorchLights torch_lights;
        const bool near_bandit_camp=std::hypot(render_cam.x-bandit_camp_center_x,render_cam.z-bandit_camp_center_z)<300.;
        const bool near_trail_camp=std::hypot(render_cam.x-WORLD_TRAIL_CAMP_X,render_cam.z-WORLD_TRAIL_CAMP_Z)<130.;
        const bool lights_valid=near_trail_camp?
            world_torch_lights_build(trail_camp_torches,1,WORLD_TRAIL_CAMP_X,WORLD_TRAIL_CAMP_Z,render_cam.x,render_cam.y,render_cam.z,visual_time,torch_lights):near_bandit_camp?
            world_torch_lights_build(bandit_torch_emitters,qa_bandit_lights_off?0:std::size(bandit_torch_emitters),
                bandit_camp_center_x,bandit_camp_center_z,render_cam.x,render_cam.y,render_cam.z,visual_time,torch_lights):
            world_torch_lights_build(town_torch_emitters.data(),city_showcase?0:town_torch_emitters.size(),
                creek_origin_x,creek_origin_z,render_cam.x,render_cam.y,render_cam.z,visual_time,torch_lights);
        if(!lights_valid) {
            std::fprintf(stderr,"[World] Invalid torch light frame\n");launch->exit_code=1;break;
        }
        if(dragon_breath){
            std::array<WorldDragonBreathLight,2> glow={};
            const auto count=world_dragon_breath_lights(dragon_breath,double(visual_time)+dragon_time_offset,glow.data(),glow.size());
            for(std::size_t i=0;i<count;i++)glow[i].strength*=7.f;
            if(!world_dragon_lights_merge(torch_lights,glow.data(),count,render_cam.x,render_cam.y,render_cam.z)){
                dragon_breath_failures++;launch->exit_code=1;running=false;
            }
        }
        studio_lights(torch_lights,render_cam,visual_time,studio.vfx_previewing?&studio.draft:nullptr,studio.vfx_previewing?studio.vfx_clock.time:-1,studio.vfx_previewing&&studio.vfx_solo?studio.particle_selection:-1);
        WorldTorchLights actor_practical_lights;
        if(!source_actor_practicals(render_cam,visual_time,actor_practical_lights)){
            std::fprintf(stderr,"[World] Invalid source actor practical frame\n");launch->exit_code=1;break;
        }
        torch_peak_lights=std::max(torch_peak_lights,size_t(std::max(torch_lights.count,actor_practical_lights.count)));
        bandit_light_count=near_bandit_camp?torch_lights.count:0;
        bandit_peak_lights=std::max(bandit_peak_lights,bandit_light_count);

        // Keep the shadow texel lattice fixed in world space during walking,
        // while all rendering remains camera-relative across the 100 km map.
        M4 lightVP;
        const double shadow_camera[]={render_cam.x,render_cam.y,render_cam.z};
        const bool house_shadow=opening_interior.enabled?
            (opening_interior.release_city_fallbacks?
                world_scenery_streaming_stats(scenery).interior_city_suspended:
                world_opening_interior::contains(opening_interior,{render_cam.x,render_cam.y,render_cam.z})):
            (opening_house_geometry&&render_cam.x>-3506&&render_cam.x<-3494&&
             render_cam.z>-11006&&render_cam.z<-10994&&render_cam.y>599&&render_cam.y<605);
        // The furnished room occupies eleven metres. Focus the existing map
        // on it instead of spending its texels over a 320-metre outdoor span.
        const float shadow_radius=house_shadow?20.f:160.f;
        if(house_shadow)++opening_house_shadow_frames;
        const float shadow_sun[]={light.sun.x,light.sun.y,light.sun.z},shadow_center[]={0,house_shadow?-1.f:-25.f,0};
        if(!world_shadow_directional_matrix(shadow_camera,shadow_sun,shadow_radius,shadow.side,shadow_center,lightVP.m)) {
            std::fprintf(stderr,"[World] Invalid directional shadow projection\n");
            launch->exit_code=1;break;
        }
        const float winter_weight=stronghold_finish?float(std::clamp((650.-std::hypot(render_cam.x-stronghold_x,render_cam.z-stronghold_z))/150.,0.,1.)):0.f;
        const float weather_weight=weather_disabled?0.f:stronghold_finish?winter_weight:rainforest_weight;
        const float village_mist=weather_disabled?0.f:.40f*world_atlas_village_weight(render_cam.x,render_cam.z);
        last_rain=weather_disabled?0.f:stronghold_finish?.40f*winter_weight:rain;
        last_fog=graphics.volumetric_fog?std::clamp(mist*std::max(
            weather_disabled?0.f:rainforest_atmosphere.mist_weight,village_mist),0.f,1.5f):0.f;
        last_wind_x=1.1f*wind*weather_weight;last_wind_z=.38f*wind*weather_weight;
        // Legacy wetness remains instantaneous; authored wetness has a bounded
        // world-anchored history shared by every material receiver.
        const float wetness=weather_disabled?0.f:std::clamp(rain,0.f,1.f);
        WorldWeatherFrame weather_view=weather_frame(render_cam,light,right,up,forward,lightVP,target.w,target.h,
            2*jitter[0]/target.w,2*jitter[1]/target.h,last_rain,last_fog,last_wind_x,last_wind_z);
        const auto effects_profile=world_environment_effects_profile(graphics.environment_effects);
        weather_view.shaft_strength=effects_profile.shaft_strength;weather_view.fog_steps=effects_profile.fog_steps;
        weather_view.authored=active_authored_weather;weather_view.fog_enabled=graphics.volumetric_fog;
        weather_view.shadow=shadow.depth;weather_view.macro_height=macro_height;
        weather_view.cover_height=LandscapeMaterials::cover_height;weather_view.cover_userdata=&landscape;
        weather_view.contact_surface=LandscapeMaterials::rain_contact;weather_view.contact_userdata=&landscape;
        if(weather_drip_cache&&weather_drips_prepared){
            // Geometry IO is confined to explicit scene commits or destination
            // loading. Walking only reselects cached source points every8m.
            if(destination_loading&&std::hypot(render_cam.x-weather_drips_origin_x,render_cam.z-weather_drips_origin_z)>120.)prepare_weather_drips(render_cam);
            world_weather_drips::refresh(*weather_drip_cache,scenery,render_cam.x,render_cam.z,weather_surface,nullptr);
            weather_view.drips=weather_drip_cache->points.data();weather_view.drip_count=weather_drip_cache->count;
        }
        world_weather_update(weather,weather_view);
        const auto cinematic_audio_flags=SDL_GetWindowFlags(window);
        const bool sound_focused=!(cinematic_audio_flags&SDL_WINDOW_MINIMIZED)&&(launch->frames||launch->benchmark||(cinematic_audio_flags&SDL_WINDOW_INPUT_FOCUS));
        if(cutscene_audio_frame(render_cam)){}
        else if(world_sound_active)world_sound_frame(studio.open?render_cam:cam,render_cam,launch->frames?1.f/60.f:dt,sound_focused,
            !studio.open&&(show_graphics||inventory_open||(presentation&&preview_screen!=1)));
        else studio.cinematic_audio_frame(render_cam,SDL_GetTicks64()*.001,!destination_loading&&sound_focused);
        const bool opening_apartment_audio=opening_enabled&&(opening_state.stage()<=world_opening_prologue::Stage::Transport||opening_state.stage()==world_opening_prologue::Stage::GameMenu);
        const float opening_score_gain=cutscene_frame.active?.18f:opening_frame.npc_intro?.18f:opening_apartment_audio?(!opening_state.config().house_shots.empty()&&opening_state.stage()!=world_opening_prologue::Stage::Transport?.10f:.35f):1.f;
        if(presentation)preview_audio.update(dt,fight.engaged(),!destination_loading&&preview_screen==1&&!show_graphics&&!inventory_open&&!(cinematic_audio_flags&SDL_WINDOW_MINIMIZED)&&(launch->frames||(cinematic_audio_flags&SDL_WINDOW_INPUT_FOCUS)),preview_settings.music*opening_score_gain);
        if(presentation){const auto *line=studio.components_runtime?studio.components_runtime->current_line():nullptr;preview_voices.update(line?world_presentation::voice_line(line->id,preview_stage()):0,!destination_loading&&(launch->frames||(cinematic_audio_flags&SDL_WINDOW_INPUT_FOCUS)),preview_screen!=1||authored_cutscene_locked,preview_settings.voice);}
        if(demo_audio) {
            const auto audio_window_flags=SDL_GetWindowFlags(window);
            const auto *audio_keys=SDL_GetKeyboardState(nullptr);
            const Camera &audio_listener=studio.open?render_cam:cam;
            WorldDemoAudioFrame audio_frame;
            audio_frame.x=audio_listener.x;audio_frame.y=audio_listener.y;audio_frame.z=audio_listener.z;audio_frame.yaw=render_cam.yaw;
            audio_frame.dt=dt;audio_frame.rain=stronghold_finish?0.f:authored_listener.rain;audio_frame.wind=wind;audio_frame.hour=hour;
            audio_frame.grounded=cam.grounded&&!cam.fly&&(!stronghold_finish||cam.z<stronghold_z+14);
            audio_frame.running=(audio_keys[SDL_SCANCODE_LSHIFT]!=0||(pad_playable&&preview_pad.sprint))&&!(opening_enabled&&opening_state.stage()<=world_opening_prologue::Stage::Transport);
            audio_frame.focused=!destination_loading&&!(audio_window_flags&SDL_WINDOW_MINIMIZED)&&
                (launch->frames||launch->benchmark||(audio_window_flags&SDL_WINDOW_INPUT_FOCUS));
            audio_frame.quiet_interior=opening_enabled&&(opening_state.stage()<=world_opening_prologue::Stage::Transport||opening_state.stage()==world_opening_prologue::Stage::GameMenu);
            audio_frame.interior=audio_frame.quiet_interior||inside_tavern_shell(audio_listener)||(town_enabled&&audio_listener.x>-1291.5&&audio_listener.x<-1276&&audio_listener.z>-10296.5&&audio_listener.z<-10277.5&&audio_listener.y>24.1&&audio_listener.y<28.5);
            audio_frame.covered=audio_frame.interior||LandscapeMaterials::cover_height(audio_listener.x,audio_listener.z,&landscape)>audio_listener.y+.4;
            world_demo_audio_set_ambience_gain(demo_audio,cutscene_frame.active?.35f:studio.cinematic_outside());
            audio_frame.paused=authored_paused;audio_frame.loading=destination_loading;
            {int mv=100,muv=100,sv=100;audio_get_volumes(&mv,&muv,&sv);audio_frame.sfx_gain=float(mv)*float(sv)/10000.f;}
            {
                const auto plants=skeletal_hero.stats().foot_plants;
                const double hx=skeletal_hero.x(),hz=skeletal_hero.z();
                const float sdt=launch->frames?1.f/60.f:dt;
                const double moved=std::hypot(hx-hero_step_x,hz-hero_step_z);
                // Placement/checkpoint jumps re-arm without a stale event.
                if(!hero_step_ready||moved>2.||!skeletal_hero_active){hero_step_ready=skeletal_hero_active;hero_step_plants=plants;hero_step_speed=0;}
                else if(sdt>0)hero_step_speed+=(float(moved/sdt)-hero_step_speed)*std::min(1.f,sdt*8.f);
                hero_step_gap=std::max(0.f,hero_step_gap-sdt);
                if(plants>hero_step_plants){
                    // Walking indoors is ~1.25 m/s; blocked pushes and turns on
                    // the spot stay below .30 m/s of actual swept travel.
                    if(hero_step_speed>.30f&&hero_step_gap<=0&&audio_frame.grounded&&!authored_paused&&!destination_loading){
                        audio_frame.step_events=1;hero_step_gap=.22f;++hero_steps_played;
                    }else ++hero_steps_filtered;
                }
                hero_step_plants=plants;hero_step_x=hx;hero_step_z=hz;
                audio_frame.contact_steps=skeletal_hero_active;
            }
            if(!world_demo_audio_update(demo_audio,audio_frame))demo_audio_update_failures++;
            if(weather_thunder_ready&&!world_demo_audio_thunder(demo_audio,authored_storm_output.thunder.data(),authored_storm_output.thunder_count,audio_frame))demo_audio_update_failures++;
        }
        g_engine_perf.raw_update_ms=millis(frame_start);
        g_engine_perf.update_ms=g_engine_perf.raw_update_ms;
        render_start=SDL_GetPerformanceCounter();
        if (profile_gpu)queries.begin(frame);

        bool view_ready = terrain.view_ready(render_cam.x, render_cam.z, render_cam.fly);
        unsigned destination_wait_mask=0;
        if(destination_loading) {
            WorldCoverMetrics staging={};world_cover_metrics(dressing.cover,&staging);
            // The editor can select and move objects while nearby HIGH detail
            // finishes streaming. Waiting for every requested mesh trapped
            // apartment return jumps behind hundreds of optional uploads.
            const bool wait_for_assets=!studio.open||!editor_preview_preparing;
            const bool blocked[]={!view_ready,wait_for_assets&&world_scenery_streaming_stats(scenery).critical_pending!=0,
                horizon.building,staging.near_building!=0,staging.tree_building!=0,wildlife_loading_frame,
                bandit_loading_frame,trail_camp_loading_frame,arrival_camp_loading_frame,dragons_loading_frame};
            for(unsigned i=0;i<10;++i)if(blocked[i]){destination_wait_mask|=1u<<i;++destination_wait_frames[i];}
            if(!story_appearance_pending.empty()){destination_wait_mask|=1u<<10;++story_appearance_loading_frames;}
            if(story_appearance_pending.empty()&&view_ready&&(!wait_for_assets||world_scenery_streaming_stats(scenery).critical_pending==0)&&!horizon.building&&!staging.near_building&&!staging.tree_building&&!wildlife_loading_frame&&!bandit_loading_frame&&!trail_camp_loading_frame&&!arrival_camp_loading_frame&&!dragons_loading_frame) {
                local_light_shadow.enabled=false; // Probe bounces never sample a stale local cube.
                const auto probe_start=SDL_GetPerformanceCounter();
                if(!ensure_local_probes(render_cam)){launch->exit_code=1;running=false;}
                destination_probe_ms+=millis(probe_start);
                const auto material_start=SDL_GetPerformanceCounter();
                if(running&&!editor_apply_reuse_materials&&!warm_main_materials(render_cam,visual_time,false)){launch->exit_code=1;running=false;}
                destination_material_ms+=millis(material_start);
                if(running){
                    destination_loading=false;
                    editor_preview_preparing=false;
                    editor_apply_reuse_materials=false;
                    if(!editor_apply_perf.empty()&&editor_apply_perf.back()[4]==0){
                        editor_apply_perf.back()[4]=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-editor_apply_wait_started).count()+editor_apply_perf.back()[1];
                        std::fprintf(stderr,"[Terrain Ready] commit=%.0f visible=%.3fms\n",editor_apply_perf.back()[0],editor_apply_perf.back()[4]);std::fflush(stderr);
                    }
                }
            }
            else view_ready=false;
        }
        if(!running)break;
        if (!view_ready) { temporal_cut=true;loading_frames++; longest_loading_streak = std::max(longest_loading_streak, ++loading_streak); }
        else loading_streak = 0;
        bool reflection_rendered=false;
        if(!view_ready||temporal_cut)quality_valid_after=std::max(quality_valid_after,frame+30);
        const auto window_flags=SDL_GetWindowFlags(window);
        const bool quality_eligible=view_ready&&frame>quality_valid_after&&!show_graphics&&!opening_frame.npc_intro&&
            !(window_flags&SDL_WINDOW_MINIMIZED)&&(launch->frames||(window_flags&SDL_WINDOW_INPUT_FOCUS));
        if(graphics.auto_quality)adaptive_queries.begin(frame,quality_epoch,0.,quality_eligible);
        const double far_camera[]={render_cam.x,render_cam.y,render_cam.z};
        const bool far_shadow_active=far_shadow.enabled&&!house_shadow&&light.sun.y>.02f;
        if (view_ready&&far_shadow_active) {
            const float far_sun[]={light.sun.x,light.sun.y,light.sun.z};
            if(far_shadow.needs_refresh(far_camera,far_sun,frame)){
                M4 farVP;const float far_center[]={0,-25.f,0};
                if(world_shadow_directional_matrix(far_camera,far_sun,far_shadow.radius,far_shadow.side,far_center,farVP.m,10.f,9000.f,4500.f)){
                    far_shadow.pending_vp=farVP;far_shadow.px=far_camera[0];far_shadow.py=far_camera[1];far_shadow.pz=far_camera[2];
                    std::copy_n(far_sun,3,far_shadow.pending_sun);far_shadow.phase=0;
                }
            }
            if(far_shadow.phase>=0) {
                const M4 farVP=far_shadow.pending(far_camera);
                // Without a valid map yet, build all three phases now.
                const int last_phase=far_shadow.valid?far_shadow.phase:2;
                far_shadow.collect();
                glBeginQuery(GL_TIME_ELAPSED,far_shadow.query);
                glBindFramebuffer(GL_FRAMEBUFFER,far_shadow.fbo[1-far_shadow.front]);glViewport(0,0,far_shadow.side,far_shadow.side);
                glEnable(GL_DEPTH_TEST);glDepthMask(GL_TRUE);glDepthFunc(GL_LESS);glDisable(GL_BLEND);
                glEnable(GL_CULL_FACE);glCullFace(GL_BACK);glEnable(GL_POLYGON_OFFSET_FILL);glPolygonOffset(2,4);
                for(int phase=far_shadow.phase;phase<=last_phase;phase++){
                    WorldModelDepthMaterialScope depth_materials(foliageShadow.id);
                    if(phase==0){
                        glClear(GL_DEPTH_BUFFER_BIT);
                        glEnable(GL_CULL_FACE);
                        terrain.draw(shadow.prog,render_cam,farVP,true,far_shadow.radius+200.f);
                        continue;
                    }
                    glDisable(GL_CULL_FACE);
                    foliageShadow.use();foliageShadow.m4("uVP",farVP);foliageShadow.f("uTime",visual_time);
                    weather_program(foliageShadow,last_wind_x,last_wind_z,wetness);
                    if(phase==1){
                        // Tree shadow streams are re-selected for this frustum; the
                        // directional pass below restores its own selection.
                        dressing.prepare_tree_shadows(farVP,render_cam);
                        dressing.draw(foliageShadow,render_cam,true,nullptr,false,true);
                        outcrops.draw(foliageShadow,render_cam);
                    } else if(!arena_enabled||arena_scenery_enabled){
                        world_scenery_shadow_clip(scenery,farVP.m);
                        world_scenery_draw(scenery,foliageShadow.id,render_cam.x,render_cam.y,render_cam.z,true);
                        world_scenery_shadow_clip(scenery,nullptr);
                        structure_gpu.draw(render_cam,farVP,farVP,light.sun,light.sunColor,light.ambient,light.fog,light.haze,0,0,true);
                    }
                }
                glEnable(GL_CULL_FACE);glDisable(GL_POLYGON_OFFSET_FILL);
                glEndQuery(GL_TIME_ELAPSED);far_shadow.query_pending=true;
                if(last_phase>=2)far_shadow.finish(frame);else far_shadow.phase=last_phase+1;
            }
        }
        const bool far_shadow_bound=far_shadow_active&&far_shadow.valid;
        const M4 far_shadow_vp=far_shadow_bound?far_shadow.current(far_camera):identity();
        const V3 far_shadow_params=far_shadow_bound?V3{far_shadow.radius*.75f,far_shadow.radius*.9f,.00017f}:V3{0,0,0};
        auto bind_far_shadow=[&](Program &program){
            glActiveTexture(GL_TEXTURE24);glBindTexture(GL_TEXTURE_2D,far_shadow_bound?far_shadow.texture():0);glActiveTexture(GL_TEXTURE0);
            program.i("uFarShadow",24);program.m4("uFarLightVP",far_shadow_vp);program.v3("uFarShadowParams",far_shadow_params);
        };
        if (view_ready) {
            dressing.prepare_tree_shadows(lightVP,render_cam);
            if (profile_gpu)shadow_queries.begin(frame);
            glBindFramebuffer(GL_FRAMEBUFFER, shadow.fbo);
            glViewport(0, 0, shadow.side, shadow.side);
            glEnable(GL_DEPTH_TEST);
            glDepthMask(GL_TRUE);
            glDisable(GL_BLEND);
            glEnable(GL_CULL_FACE);
            glCullFace(GL_BACK);
            glEnable(GL_POLYGON_OFFSET_FILL);
            glPolygonOffset(2, 3);
            glClear(GL_DEPTH_BUFFER_BIT);
            {
            WorldModelDepthMaterialScope depth_materials(foliageShadow.id);
            terrain.draw(shadow.prog, render_cam, lightVP, true);
            glDisable(GL_CULL_FACE);
            foliageShadow.use();
            foliageShadow.m4("uVP", lightVP);
            foliageShadow.f("uTime", visual_time);
            weather_program(foliageShadow,last_wind_x,last_wind_z,wetness);
            dressing.draw(foliageShadow, render_cam, true);
            outcrops.draw(foliageShadow,render_cam);
            if(!arena_enabled||arena_scenery_enabled){
                world_scenery_shadow_clip(scenery,authored_shadow_clip_enabled?lightVP.m:nullptr);
                world_scenery_draw(scenery,foliageShadow.id,render_cam.x,render_cam.y,render_cam.z,true);
                world_scenery_shadow_clip(scenery,nullptr);
                structure_gpu.draw(render_cam,lightVP,lightVP,light.sun,light.sunColor,light.ambient,light.fog,light.haze,0,0,true);
            }
            if(world_village_population_stats(population).visible||(world_road_traffic_count(road_traffic)>0)){
                auto &p=live_programs.crowd_shadow;p.use();p.m4("uVP",lightVP);p.f("uTime",visual_time);weather_program(p,last_wind_x,last_wind_z,wetness);
                WorldModelDepthMaterialScope character_depth(p.id);world_village_population_draw(population,p.id,render_cam.x,render_cam.y,render_cam.z,true);foliageShadow.use();
                world_road_traffic_draw(road_traffic,p.id,render_cam.x,render_cam.y,render_cam.z,render_cam.yaw,render_cam.pitch,render_cam.aspect,2,lightVP.m);foliageShadow.use();
            }
            world_castle_training_draw(training,foliageShadow.id,render_cam.x,render_cam.y,render_cam.z,true);
            draw_player(foliageShadow.id,render_cam,true);
            world_village_watch_draw(watch,foliageShadow.id,render_cam.x,render_cam.y,render_cam.z,true);
            world_village_pets_draw(pets,foliageShadow.id,render_cam.x,render_cam.y,render_cam.z,true);
            world_wildlife_draw(wildlife,foliageShadow.id,render_cam.x,render_cam.y,render_cam.z,render_cam.yaw,render_cam.pitch,render_cam.aspect,2);
            world_bandit_camp_draw(bandits,foliageShadow.id,render_cam.x,render_cam.y,render_cam.z,render_cam.yaw,render_cam.pitch,render_cam.aspect,2);
            world_stronghold_draw(stronghold,foliageShadow.id,render_cam.x,render_cam.y,render_cam.z,2,lightVP.m);
            world_trail_camp_draw(trail_camp,foliageShadow.id,render_cam.x,render_cam.y,render_cam.z,render_cam.yaw,render_cam.pitch,render_cam.aspect,2);
        world_trail_camp_draw(arrival_camp,foliageShadow.id,render_cam.x,render_cam.y,render_cam.z,render_cam.yaw,render_cam.pitch,render_cam.aspect,2);
        world_trail_camp_draw(first_journey_townsfolk,foliageShadow.id,render_cam.x,render_cam.y,render_cam.z,render_cam.yaw,render_cam.pitch,render_cam.aspect,2);
        battlefield_draw_arrows(foliageShadow,render_cam,true);
            world_castle_dragons_draw(dragons,foliageShadow.id,render_cam.x,render_cam.y,render_cam.z,render_cam.yaw,render_cam.pitch,render_cam.aspect,2);
            if(!draw_live_shadow(render_cam,lightVP,visual_time)){launch->exit_code=1;running=false;}
            }
            glEnable(GL_CULL_FACE);
            glDisable(GL_POLYGON_OFFSET_FILL);
            const bool practical_shadows=local_light_shadows_supported&&contact_enabled&&graphics.contact_shadows&&
                !std::getenv("VALWOOD_QA_DISABLE_LOCAL_LIGHT_SHADOWS");
            if(!local_light_shadow.select(torch_lights,render_cam,practical_shadows,graphics.shadow_side>=4096?1024:512)){
                std::fprintf(stderr,"[Local shadows] Cube allocation failed\n");launch->exit_code=1;running=false;
            }
            if(running&&!local_light_shadow.update(render_cam,[&](const M4 &pointVP){
                WorldModelDepthMaterialScope materials(foliageShadow.id);
                terrain.draw(shadow.prog,render_cam,pointVP,true);
                foliageShadow.use();foliageShadow.m4("uVP",pointVP);foliageShadow.f("uTime",visual_time);
                weather_program(foliageShadow,last_wind_x,last_wind_z,wetness);
                world_scenery_shadow_clip(scenery,pointVP.m);
                if(!arena_enabled||arena_scenery_enabled)world_scenery_draw(scenery,foliageShadow.id,render_cam.x,render_cam.y,render_cam.z,true);
                world_scenery_shadow_clip(scenery,nullptr);
                structure_gpu.draw(render_cam,pointVP,pointVP,light.sun,light.sunColor,light.ambient,light.fog,light.haze,0,0,true);
                foliageShadow.use();foliageShadow.m4("uVP",pointVP);
                // Generated tree shadow streams are rebuilt per cube face, then
                // restored by the next directional pass. Grass is not a caster.
                dressing.prepare_tree_shadows(pointVP,render_cam);dressing.draw(foliageShadow,render_cam,true,nullptr,false,true);
                outcrops.draw(foliageShadow,render_cam);
                if(world_village_population_stats(population).visible||(world_road_traffic_count(road_traffic)>0)){auto &p=live_programs.crowd_shadow;p.use();p.m4("uVP",pointVP);p.f("uTime",visual_time);weather_program(p,last_wind_x,last_wind_z,wetness);WorldModelDepthMaterialScope actor_materials(p.id);world_village_population_draw(population,p.id,render_cam.x,render_cam.y,render_cam.z,true);world_road_traffic_draw(road_traffic,p.id,render_cam.x,render_cam.y,render_cam.z,render_cam.yaw,render_cam.pitch,render_cam.aspect,2,pointVP.m);}
                foliageShadow.use();foliageShadow.m4("uVP",pointVP);
                world_castle_training_draw(training,foliageShadow.id,render_cam.x,render_cam.y,render_cam.z,true);
                draw_player(foliageShadow.id,render_cam,true);
                world_village_watch_draw(watch,foliageShadow.id,render_cam.x,render_cam.y,render_cam.z,true);
                world_trail_camp_draw(arrival_camp,foliageShadow.id,render_cam.x,render_cam.y,render_cam.z,render_cam.yaw,render_cam.pitch,render_cam.aspect,2);
                world_trail_camp_draw(trail_camp,foliageShadow.id,render_cam.x,render_cam.y,render_cam.z,render_cam.yaw,render_cam.pitch,render_cam.aspect,2);
                world_bandit_camp_draw(bandits,foliageShadow.id,render_cam.x,render_cam.y,render_cam.z,render_cam.yaw,render_cam.pitch,render_cam.aspect,2);
                world_stronghold_draw(stronghold,foliageShadow.id,render_cam.x,render_cam.y,render_cam.z,2,pointVP.m);
                return draw_live_shadow(render_cam,pointVP,visual_time);
            })){launch->exit_code=1;running=false;}
            if (profile_gpu)shadow_queries.end();
            g_engine_perf.shadow_gpu_ms=shadow_queries.milliseconds;
            const auto water_detail=world_water_quality_profile(graphics.water_quality);
            const bool planar_reflections=graphics.reflections&&water_detail.planar;
            auto local_sample=world_atlas_sample(render_cam.x,render_cam.z);
            const auto *pool=world_atlas_rainforest_pool();
            V3 pool_delta={float(pool->x-render_cam.x),float(pool->surface_y-render_cam.y),float(pool->z-render_cam.z)};
            const float pool_forward=dot(pool_delta,forward),pool_right=dot(pool_delta,right),pool_up=dot(pool_delta,up);
            // Conservative camera-frustum test: skip the costly mirror when the
            // small pond is behind us or outside the image, with a radius margin.
            const bool draw_pool=std::hypot(pool_delta.x,pool_delta.z)<220.f&&pool_forward>-20.f&&
                std::abs(pool_right)<std::max(0.f,pool_forward)*std::tan(render_cam.fov*.5f)*render_cam.aspect+20.f&&
                std::abs(pool_up)<std::max(0.f,pool_forward)*std::tan(render_cam.fov*.5f)+20.f;
            const bool draw_creek=!city_showcase&&castle_creek_visible(render_cam,forward,right,up);
            float water_level=draw_pool?float(pool->surface_y):draw_creek?float(WORLD_CASTLE_CREEK_SURFACE_Y):0.f;
            bool authored_reflection=false,authored_same_plane=false,river_reflection=false;
            const double custom_water_distance=studio.open?4096.:1200.;
            double nearest_water=draw_pool?std::max(0.,std::hypot(pool_delta.x,pool_delta.z)-std::max(pool->radius_x,pool->radius_z)):1e30;
            if(draw_creek&&!draw_pool){double ox,oz;world_atlas_village_origin(&ox,&oz);nearest_water=std::hypot(render_cam.x-std::clamp(render_cam.x,ox+WORLD_CASTLE_CREEK_MIN_X,ox+WORLD_CASTLE_CREEK_MAX_X),render_cam.z-std::clamp(render_cam.z,oz+WORLD_CASTLE_CREEK_MIN_Z,oz+WORLD_CASTLE_CREEK_MAX_Z));}
            // Share the existing single mirror target with the nearest visible
            // authored pond. Hidden/disabled/off-screen water cannot take it.
            // Sloping rivers may share this target only near a gentle local
            // plane. Their shader fades to the sky outside that plane's bounds.
            if(planar_reflections)for(unsigned i=0;i<studio.live.waters.size();++i){
                const auto &w=studio.live.waters[i];
                if(w.source!=3||!w.enabled||!world_entity::visible(studio.live.entities,{world_entity::Kind::Water,i})||render_cam.y<=w.level+.02)continue;
                const double reach=std::max(double(w.radius_x),double(w.radius_z));
                const double distance=std::max(0.,std::hypot(render_cam.x-w.x,render_cam.z-w.z)-reach);
                if(distance>220.||std::hypot(render_cam.x-w.x,render_cam.z-w.z)>=custom_water_distance)continue;
                WorldPoolMainClipInput input;std::memcpy(input.vp,vp.m,sizeof(input.vp));
                input.pool_bounds[0]=float(w.x);input.pool_bounds[1]=float(w.z);input.pool_bounds[2]=1/w.radius_x;input.pool_bounds[3]=1/w.radius_z;
                input.origin[0]=float(render_cam.x);input.origin[1]=float(render_cam.z);input.offset_y=float(-render_cam.y);input.water_level=float(w.level);
                if(world_pool_main_quad_outside_clip(input))continue;
                if(float(w.level)==water_level)authored_same_plane=true;
                if(distance<nearest_water){nearest_water=distance;water_level=float(w.level);authored_reflection=true;}
            }
            if(planar_reflections&&world_water_style_enabled(studio.live,world_water_source_style(studio.live,4))&&
                    studio.live.surface_field&&studio.live.surface_field->river_network) {
                const auto &network=*studio.live.surface_field->river_network;
                for(const auto &s:network.segments) {
                    const double dx=s.b.x-s.a.x,dz=s.b.z-s.a.z;
                    const double t=std::clamp(((render_cam.x-s.a.x)*dx+(render_cam.z-s.a.z)*dz)/(s.length*s.length),0.,1.);
                    const double x=s.a.x+dx*t,z=s.a.z+dz*t;
                    const double distance=std::max(0.,std::hypot(render_cam.x-x,render_cam.z-z)-s.half);
                    if(distance>=nearest_water||distance>120.)continue;
                    const auto hit=network.surface(x,z);
                    if(!hit.inside||hit.grade>.02||render_cam.y<=hit.height+.04)continue;
                    WorldPoolMainClipInput input;std::memcpy(input.vp,vp.m,sizeof(input.vp));
                    const float radius=float(std::max(2.,s.half));
                    input.pool_bounds[0]=float(x);input.pool_bounds[1]=float(z);input.pool_bounds[2]=1/radius;input.pool_bounds[3]=1/radius;
                    input.origin[0]=float(render_cam.x);input.origin[1]=float(render_cam.z);input.offset_y=float(-render_cam.y);input.water_level=float(hit.height);
                    if(world_pool_main_quad_outside_clip(input))continue;
                    nearest_water=distance;water_level=float(hit.height);river_reflection=true;authored_reflection=false;
                }
            }
            const bool legacy_ocean_candidate=render_cam.y<650.&&local_sample.shore_distance<2400.&&render_cam.pitch<.65f;
            const bool ocean_enabled=!studio_water_features||
                world_water_style_enabled(studio.live,world_water_source_style(studio.live,0));
            const bool local_mirror_consumer=authored_reflection||river_reflection||draw_creek||draw_pool;
            const auto reflection_demand=world_water_reflection_demand(planar_reflections,
                local_mirror_consumer,legacy_ocean_candidate,ocean_enabled,force_legacy_ocean_reflection);
            bool reflect_water=reflection_demand.render;
            if(planar_reflections&&legacy_ocean_candidate)++ocean_legacy_candidate_frames;
            if(!ocean_enabled)++ocean_disabled_frames;
            if(planar_reflections&&local_mirror_consumer)++local_mirror_consumer_frames;
            if(reflection_demand.skipped_unused_ocean)++unused_ocean_mirror_skips;
            // Restrict the built-in pool only when it is the sole mirror
            // consumer. Authored water at this plane needs the complete image.
            // Loading warm passes retain their original built-in owners.
            if(reflect_water&&draw_pool&&!authored_reflection&&!authored_same_plane&&!river_reflection&&water_level==float(pool->surface_y)&&
                    water_level!=0.f&&water_level!=float(WORLD_CASTLE_CREEK_SURFACE_Y)) {
                WorldPoolMainClipInput input;
                std::memcpy(input.vp,vp.m,sizeof(input.vp));
                input.pool_bounds[0]=float(pool->x);input.pool_bounds[1]=float(pool->z);
                input.pool_bounds[2]=float(1/pool->radius_x);input.pool_bounds[3]=float(1/pool->radius_z);
                input.origin[0]=float(render_cam.x);input.origin[1]=float(render_cam.z);
                input.offset_y=float(-render_cam.y);input.water_level=water_level;
                pool_reflection_clip_tests++;
                if(world_pool_main_quad_outside_clip(input)) {
                    pool_reflection_clip_outside++;
                    if(pool_reflection_clip_enabled){reflect_water=false;pool_reflection_clip_skips++;}
                }
            }
            // A distant same-plane pond can still sample this mirror even
            // when it did not win nearest-water selection. Certify ALL draws.
            bool sole_river_consumer=river_reflection&&!draw_pool&&!draw_creek&&water_level!=0.f;
            if(sole_river_consumer)for(unsigned i=0;i<studio.live.waters.size();++i){const auto&w=studio.live.waters[i];
                if(w.source==3&&w.enabled&&world_entity::visible(studio.live.entities,{world_entity::Kind::Water,i})&&float(w.level)==water_level&&std::hypot(render_cam.x-w.x,render_cam.z-w.z)<custom_water_distance){sole_river_consumer=false;break;}}
            M4 reflection_vp=vp;
            if (profile_gpu) reflection_queries.begin(frame);
            // Water detail controls actual reflected geometry/image work;
            // the advanced reflection resolution remains an independent cap.
            const float legacy_scale=graphics.reflection_scale*(river_reflection&&graphics.water_quality!=WorldWaterQuality::Ultra?.5f:1.f);
            const float mirror_scale=std::min(legacy_scale,water_detail.mirror_scale_cap);
            const int reflection_width=std::max(1,int(std::lround(target.w*mirror_scale)));
            const int reflection_height=std::max(1,int(std::lround(target.h*mirror_scale)));
            const bool reflection_needs_allocation=reflection.w!=reflection_width||reflection.h!=reflection_height||!reflection.fbo;
            // Reuse the image together with the mirrored projection that made
            // it. The main-camera VP is never a valid substitute on alternate
            // frames. Refresh on real motion: the old scissored image cannot
            // supply newly exposed geometry, even with a rebased projection.
            const WorldWaterMirrorCache::Pose mirror_pose={render_cam.x,render_cam.y,render_cam.z,
                render_cam.yaw,render_cam.pitch,render_cam.fov,render_cam.aspect,water_level,
                reflection_width,reflection_height};
            const unsigned mirror_capture_period=graphics.water_quality==WorldWaterQuality::Medium?4u:
                graphics.water_quality==WorldWaterQuality::Ultra?1u:2u;
            const bool reuse_mirror=reflect_water&&!reflection_needs_allocation&&
                water_mirror_cache.can_reuse(frame,mirror_pose,
                    std::getenv("VALWOOD_QA_MIRROR_EVERY_FRAME")!=nullptr,temporal_cut,mirror_capture_period);
            if(reuse_mirror){
                const auto cached_projection=water_mirror_cache.rebased_projection(mirror_pose);
                std::memcpy(reflection_vp.m,cached_projection.data(),sizeof(reflection_vp.m));
                reflection_rendered=true;++water_mirror_rendered_frames;++water_mirror_reused_frames;
            }
            else if(reflect_water&&reflection.resize(reflection_width,reflection_height)) {
                if(!first_reflection_frame)first_reflection_frame=frame;
                if(reflection_needs_allocation)reflection_play_allocations++;
                reflection_rendered=true;
                ++water_mirror_rendered_frames;
                if(!draw_water_reflection(render_cam,light,torch_lights,lightVP,visual_time,water_level,
                        last_wind_x,last_wind_z,wetness,qa_fixed_time?0.f:std::clamp(visual_time-previous_rendered_visual_time,0.f,.1f),
                        reflection_vp,true,frame,!authored_reflection&&!authored_same_plane&&!river_reflection,sole_river_consumer,&vp)) {water_mirror_cache.invalidate();launch->exit_code=1;running=false;}
                else {
                    std::array<float,16> captured_projection;
                    std::memcpy(captured_projection.data(),reflection_vp.m,sizeof(reflection_vp.m));
                    water_mirror_cache.commit(frame,mirror_pose,captured_projection);
                    ++water_mirror_captured_frames;
                }

            } else {reflect_water=false;water_mirror_cache.invalidate();}
            if (profile_gpu) reflection_queries.end();
            g_engine_perf.reflection_gpu_ms=reflection_queries.milliseconds;
            glBindFramebuffer(GL_FRAMEBUFFER, target.fbo);
            glViewport(0,0,target.w,target.h);
            const GLenum color_buffers[]={GL_COLOR_ATTACHMENT0,GL_COLOR_ATTACHMENT1};
            glDrawBuffers(2,color_buffers);glClearColor(.5,.6,.7,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
            const float no_indirect[4]={};glClearBufferfv(GL_COLOR,1,no_indirect);
            glDrawBuffer(GL_COLOR_ATTACHMENT0);
            if (profile_gpu)sky_queries.begin(frame);
            WorldSkyFrame main_sky=sky_frame(render_cam,light,right,up,forward,2*jitter[0]/target.w,2*jitter[1]/target.h);main_sky.volumetric_clouds=graphics.clouds;
            if(!world_sky_draw(sky,main_sky,target.fbo,target.w,target.h,0)) {
                launch->exit_code=1;running=false;
            }
            if (profile_gpu)sky_queries.end();
            glDrawBuffers(2,color_buffers);
            glDepthMask(GL_TRUE);
            glEnable(GL_DEPTH_TEST);
            glEnable(GL_CULL_FACE);
            auto prepare_main_foliage_program=[&](Program &program) {
                program.use();program.i("uArchitectureWeathering",1);program.i("uShadowGather",shadow_gather_enabled?1:0);program.i("uFacadeMode",0);
                program.i("uColorCoverageCull",zero_coverage_cull_enabled?1:0);
                landscape.bind_shelter(program);studio_surface_gpu.bind(program,cam.x,cam.y,cam.z);
                program.i("uClipBelowWater",0);program.f("uWaterClipLevel",0);
                program.m4("uVP",vp);program.m4("uLightVP",lightVP);
                light_program(program,light,render_cam);torch_program(program,torch_lights);
                const bool camera_foliage=active_follow_camera&&!studio.open&&
                    !opening_camera_locked&&!preview_line&&
                    (&program==&foliage||&program==foliage.generated_grass.get()||&program==foliage.vegetation.get()||&program==foliage.depth_prepass.get());
                program.v2("uCameraFoliageFade",camera_foliage?.35f:0.f,camera_foliage?.95f:0.f);
                bind_local_probes(program,render_cam);
                weather_program(program,last_wind_x,last_wind_z,wetness);
                program.i("uWindHistoryEnabled",1);
                program.f("uWindFrameDelta",qa_fixed_time?0.f:
                    std::clamp(visual_time-previous_rendered_visual_time,0.f,.1f));
                program.f("uTemporalPhase",temporal?float(temporal_sample%8)/8.f:0.f);
                program.f("uTime",visual_time);
                glActiveTexture(GL_TEXTURE7);glBindTexture(GL_TEXTURE_2D,shadow.depth);program.i("uShadow",7);
                program.f("uPixelScale",float(target.h)/(2*std::tan(render_cam.fov*.5f)));
                program.v2("uWorldSize",float(world_atlas_width()),float(world_atlas_depth()));
                glActiveTexture(GL_TEXTURE11);glBindTexture(GL_TEXTURE_2D,macro_height);program.i("uMacroHeight",11);
                bind_far_shadow(program);
            };
            auto prepare_main_foliage=[&](){prepare_main_foliage_program(foliage);};
            const bool early_opaque=inside_tavern_shell(render_cam);
            // Authored forest rocks, trunks and understory can occlude large
            // areas of terrain/procedural grass just like village buildings.
            // Submit them once before that work, with identical materials and
            // targets; retain the same distance-selected shadow caster set.
            const bool authored_first=authored_first_enabled&&
                (city_showcase||stronghold_finish||world_atlas_village_weight(render_cam.x,render_cam.z)>0.f||world_atlas_rainforest_weight(render_cam.x,render_cam.z)>0.f);
            if(early_opaque||authored_first) {
                // Shade the opaque room once, with the exact normal color
                // program and MRT outputs. Its depth rejects hidden outside
                // terrain/vegetation; all openings remain real mesh voids.
                glDisable(GL_CULL_FACE);glDisable(GL_BLEND);
                prepare_main_foliage();
                if (profile_gpu)early_opaque_queries.begin(frame);
                // Authored architecture/props write their normal color and
                // depth once. That depth rejects hidden procedural vegetation
                // before its expensive masked PBR shading, with real openings.
                // AAA merge: authored scenery has no hair/skin materials; use
                // the lighter vegetation variant when available.
                Program &scenery_program=(lean_vegetation_ok()&&foliage.vegetation?*foliage.vegetation:foliage);
                if(&scenery_program!=&foliage)prepare_main_foliage_program(scenery_program);
                scenery_program.i("uSourceMaterialAO",city_showcase?1:0);
                if((!arena_enabled||arena_scenery_enabled)&&!(std::getenv("VALWOOD_QA_SKIP_DRAW")&&std::strstr(std::getenv("VALWOOD_QA_SKIP_DRAW"),"scenery")))world_scenery_draw_filtered(scenery,scenery_program.id,render_cam.x,render_cam.y,render_cam.z,false,
                    authored_first?nullptr:"village_tavern",nullptr);
                scenery_program.i("uSourceMaterialAO",0);foliage.use();
                foliage.i("uSourceMaterialAO",0);
                if (profile_gpu)early_opaque_queries.end();
                if(early_opaque)early_opaque_frames++;
                if(authored_first)authored_first_frames++;
                glEnable(GL_CULL_FACE);
            }
            land.use();
            land.i("uClipBelowWater",0);land.f("uWaterClipLevel",0);
            weather_program(land,last_wind_x,last_wind_z,wetness);
            light_program(land, light,render_cam);torch_program(land,torch_lights);
            bind_local_probes(land,render_cam);
            land.v2("uWorldSize", float(world_atlas_width()), float(world_atlas_depth()));
            land.v3("uEye", {float(render_cam.x), float(render_cam.y), float(render_cam.z)});
            land.m4("uLightVP", lightVP);
            const char *names[] = {"uGrass", "uRock", "uSand", "uDirt", "uGrassNormal", "uRockNormal", "uClimate", "uShadow"};
            for (int i = 0; i < 8; i++) {
                glActiveTexture(GL_TEXTURE0 + i);
                glBindTexture(GL_TEXTURE_2D, i == 7 ? shadow.depth : materials[i]);
                land.i(names[i], i);
            }
            for (int i = 8; i < 10; i++) {
                glActiveTexture(GL_TEXTURE0+i);
                glBindTexture(GL_TEXTURE_2D,materials[i]);
                land.i(i == 8 ? "uGrassARM" : "uRockARM",i);
            }
            glActiveTexture(GL_TEXTURE11);glBindTexture(GL_TEXTURE_2D,macro_height);land.i("uMacroHeight",11);
            bind_far_shadow(land);
            landscape.bind(land);studio_surface_gpu.bind(land,cam.x,cam.y,cam.z);studio.surface_terrain_binding(land);
            world_snow_bind_terrain(snow,land.id);
            showcase_terrain.bind(land.id);
            if (profile_gpu)terrain_queries.begin(frame);
            terrain.draw(land, render_cam, vp);
            if (profile_gpu)terrain_queries.end();
            // Swimming bodies belong in the refracted opaque snapshot. Drawing
            // them after water hides every submerged limb behind surface depth.
            const bool aquatic_player_before_water=(aquatic_scene||story_native_swim())&&cam.swimming&&draw_avatar&&!studio.open;
            if(aquatic_player_before_water){
                glEnable(GL_CULL_FACE);prepare_main_foliage();
                if(source_flames_enabled){torch_program(foliage,actor_practical_lights);foliage.i("uActorLighting",1);}
                draw_player(foliage.id,render_cam,false,draw_avatar);
                if(story_native_swim()&&skeletal_hero_draw){
                    const auto before=skeletal_hero.stats().draws;
                    if(!draw_live_color(render_cam,vp,lightVP,light,torch_lights,visual_time,wetness,false,0,true,true)){launch->exit_code=1;running=false;}
                    story_count_color_draw(before);
                }
                // Live hero drawing binds its skeletal program. Restore the
                // foliage owner before updating its cached uniform locations.
                foliage.use();
                if(source_flames_enabled){foliage.i("uActorLighting",0);torch_program(foliage,torch_lights);}
            }
            if(aquatic_renderer_ready&&!studio.open){
                glUseProgram(0);glBindVertexArray(0);glBindBuffer(GL_ARRAY_BUFFER,0);glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LESS);glDepthMask(GL_TRUE);glDisable(GL_BLEND);glDisable(GL_CULL_FACE);
                aquatic_renderer.draw_fish(aquatic_system,aquatic_frame(render_cam,light,vp));
            }
            // AAA merge: spline bridges/walls join the opaque snapshot so pier
            // bases stay visible through the water instead of ending at its surface.
            structure_gpu.draw(render_cam,vp,lightVP,light.sun,light.sunColor,light.ambient,light.fog,light.haze,shadow.depth,shadow.side);
            // Snapshot opaque attachments before water writes the main target.
            // Inland/ridge views skip this bandwidth and lazy allocation.
            if (profile_gpu)water_queries.begin(frame);
            draw_water_layers(render_cam,light,torch_lights,lightVP,vp,reflection_vp,visual_time,water_level,
                last_wind_x,last_wind_z,wetness,last_rain,jitter,reflect_water,draw_pool,draw_creek,true,river_reflection);
            if (profile_gpu)water_queries.end();
            if (profile_gpu)foliage_queries.begin(frame);
            if(foliage.generated_grass&&dressing.scanned_grass_count)prepare_main_foliage_program(*foliage.generated_grass);
            prepare_main_foliage();
            // Masked vegetation is the dominant overdraw. Resolve its exact
            // coverage into depth first (same programs and uniforms, colour
            // writes off, shader returns after the cutout), then shade only
            // the surviving layer with LEQUAL.
            const bool foliage_prepass=foliage_depth_prepass&&!studio.open;
            // AAA merge: coverage-only depth prepass over all masked vegetation and
            // authored scenery, then shade with depth writes off (LEQUAL) so the
            // heavy material shader runs once per visible pixel via early-Z.
            const bool prepass_scenery=foliage_prepass&&!authored_first&&!early_opaque&&(!arena_enabled||arena_scenery_enabled);
            if(foliage_prepass&&foliage.depth_prepass) {
                Program &pre=*foliage.depth_prepass;
                prepare_main_foliage_program(pre);
                glColorMask(GL_FALSE,GL_FALSE,GL_FALSE,GL_FALSE);glDepthMask(GL_TRUE);glDepthFunc(GL_LESS);
                dressing.draw(pre,render_cam,false,nullptr,false);
                outcrops.draw(pre,render_cam);
                horizon.draw(pre,render_cam);
                if(prepass_scenery){pre.i("uSourceMaterialAO",city_showcase?1:0);world_scenery_draw(scenery,pre.id,render_cam.x,render_cam.y,render_cam.z);pre.i("uSourceMaterialAO",0);}
                glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
                glDepthMask(GL_FALSE);glDepthFunc(GL_LEQUAL);
                foliage.use();
                foliage_prepass_frames++;
            }
            {
                Program &vegetation_program=(lean_vegetation_ok()&&foliage.vegetation?*foliage.vegetation:foliage);
                if(&vegetation_program!=&foliage)prepare_main_foliage_program(vegetation_program);
                static const char *qa_skip=std::getenv("VALWOOD_QA_SKIP_DRAW");
                auto qa_skipped=[&](const char *k){return qa_skip&&std::strstr(qa_skip,k);};
                if(!qa_skipped("dressing"))dressing.draw(vegetation_program, render_cam,false,qa_skipped("grass")?nullptr:foliage.generated_grass.get());
                if(!qa_skipped("outcrops"))outcrops.draw(vegetation_program,render_cam);
                if(!qa_skipped("horizon"))horizon.draw(vegetation_program,render_cam);
                foliage.use();
            }
            if(foliage_prepass&&!prepass_scenery){glDepthMask(GL_TRUE);glDepthFunc(GL_LESS);}
            if(authored_first) {
                if(early_opaque)late_opaque_skipped_frames++;
            } else if(early_opaque) {
                foliage.i("uSourceMaterialAO",city_showcase?1:0);
                world_scenery_draw_filtered(scenery,foliage.id,render_cam.x,render_cam.y,render_cam.z,false,nullptr,"village_tavern");
                foliage.i("uSourceMaterialAO",0);
                late_opaque_skipped_frames++;
            } else {
                Program &scenery_program=(lean_vegetation_ok()&&foliage.vegetation?*foliage.vegetation:foliage);
                if(&scenery_program!=&foliage)prepare_main_foliage_program(scenery_program);
                scenery_program.i("uSourceMaterialAO",city_showcase?1:0);if((!arena_enabled||arena_scenery_enabled)&&!(std::getenv("VALWOOD_QA_SKIP_DRAW")&&std::strstr(std::getenv("VALWOOD_QA_SKIP_DRAW"),"scenery")))world_scenery_draw(scenery,scenery_program.id,render_cam.x,render_cam.y,render_cam.z);scenery_program.i("uSourceMaterialAO",0);
                foliage.use();
            }
            if(prepass_scenery){glDepthMask(GL_TRUE);glDepthFunc(GL_LESS);}
            if(world_village_population_stats(population).visible){prepare_main_foliage_program(live_programs.crowd);draw_population_color(live_programs.crowd,render_cam,vp);foliage.use();}
            if((world_road_traffic_count(road_traffic)>0)){prepare_main_foliage_program(live_programs.crowd);world_road_traffic_draw(road_traffic,live_programs.crowd.id,render_cam.x,render_cam.y,render_cam.z,render_cam.yaw,render_cam.pitch,render_cam.aspect,0,vp.m);foliage.use();}
            world_castle_training_draw(training,foliage.id,render_cam.x,render_cam.y,render_cam.z);
            if(source_flames_enabled){torch_program(foliage,actor_practical_lights);foliage.i("uActorLighting",1);}
            if(!aquatic_player_before_water)draw_player(foliage.id,render_cam,false,draw_avatar);
            if(!authored_cutscene_locked&&arena_bow()&&!weapon_sheathed&&!third_person&&arena_game.state()!=world_arena_combat::State::Lost){Camera flat=render_cam;flat.pitch=0;foliage.m4("uVP",product(perspective(1.08f,render_cam.aspect,view_near_plane(),15000.f),look({0,0,0},flat.forward(),{0,1,0})));glDisable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);if(arena_player_bank)world_combat_motion_draw_bow_actor(arena_player_bank,foliage.id,render_cam.x,render_cam.y,render_cam.z,true,false);else world_arena_render_draw_bow_actor(arena_renderer,foliage.id,render_cam.x,render_cam.y,render_cam.z,true,false);world_arena_render_draw_bow(arena_renderer,foliage.id,render_cam.x,render_cam.y,render_cam.z,false);glDepthMask(GL_TRUE);glEnable(GL_DEPTH_TEST);foliage.m4("uVP",vp);}
            if(source_flames_enabled){foliage.i("uActorLighting",0);torch_program(foliage,torch_lights);}
            world_village_watch_draw(watch,foliage.id,render_cam.x,render_cam.y,render_cam.z);
            world_village_pets_draw(pets,foliage.id,render_cam.x,render_cam.y,render_cam.z);
            world_wildlife_draw(wildlife,foliage.id,render_cam.x,render_cam.y,render_cam.z,render_cam.yaw,render_cam.pitch,render_cam.aspect,std::tan(render_cam.fov*.5f));
            world_bandit_camp_draw(bandits,foliage.id,render_cam.x,render_cam.y,render_cam.z,render_cam.yaw,render_cam.pitch,render_cam.aspect,std::tan(render_cam.fov*.5f));
            if(source_flames_enabled)torch_program(foliage,actor_practical_lights);
            world_stronghold_draw(stronghold,foliage.id,render_cam.x,render_cam.y,render_cam.z,0,vp.m);
            if(source_flames_enabled)torch_program(foliage,torch_lights);
            world_trail_camp_draw(trail_camp,foliage.id,render_cam.x,render_cam.y,render_cam.z,render_cam.yaw,render_cam.pitch,render_cam.aspect,std::tan(render_cam.fov*.5f));
        world_trail_camp_draw(arrival_camp,foliage.id,render_cam.x,render_cam.y,render_cam.z,render_cam.yaw,render_cam.pitch,render_cam.aspect,std::tan(render_cam.fov*.5f));
        world_trail_camp_draw(first_journey_townsfolk,foliage.id,render_cam.x,render_cam.y,render_cam.z,render_cam.yaw,render_cam.pitch,render_cam.aspect,std::tan(render_cam.fov*.5f));
        battlefield_draw_arrows(foliage,render_cam);
            world_castle_dragons_draw(dragons,foliage.id,render_cam.x,render_cam.y,render_cam.z,render_cam.yaw,render_cam.pitch,render_cam.aspect,std::tan(render_cam.fov*.5f));
            const auto story_draws_before=skeletal_hero.stats().draws;
            if(!draw_live_color(render_cam,vp,lightVP,light,torch_lights,visual_time,wetness,false,0,!(aquatic_player_before_water&&story_native_swim()),false)){launch->exit_code=1;running=false;}
            story_count_color_draw(story_draws_before);
            const float snow_right[]={right.x,right.y,right.z},snow_up[]={up.x,up.y,up.z};
            const float snow_sky[]={light.skyLight.x,light.skyLight.y,light.skyLight.z},snow_sun[]={light.sunColor.x,light.sunColor.y,light.sunColor.z};
            world_snow_draw(snow,vp.m,render_cam.x,render_cam.y,render_cam.z,snow_right,snow_up,snow_sky,snow_sun);
            dragon_peak_visible=std::max(dragon_peak_visible,world_castle_dragons_stats(dragons).visible);
            bandit_peak_visible=std::max(bandit_peak_visible,world_bandit_camp_stats(bandits).visible);
            wildlife_peak_visible=std::max(wildlife_peak_visible,world_wildlife_stats(wildlife).visible);
            if(!world_torch_flames_draw(torch_flames,torch_frame(render_cam,light,right,up,forward,vp,target.fbo,target.w,target.h))) {
                torch_failures++;launch->exit_code=1;running=false;
            }
            torch_main_frames++;
            torch_peak_visible=std::max(torch_peak_visible,size_t(world_torch_flames_stats(torch_flames).visible_emitters));
            glDrawBuffers(2,color_buffers);glDepthMask(GL_TRUE);

            if(aquatic_renderer_ready&&!studio.open){
                glDrawBuffer(GL_COLOR_ATTACHMENT0);glUseProgram(0);glBindVertexArray(0);glBindBuffer(GL_ARRAY_BUFFER,0);glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LESS);glDepthMask(GL_TRUE);glDisable(GL_BLEND);glDisable(GL_CULL_FACE);
                aquatic_renderer.draw_surface(aquatic_system,aquatic_frame(render_cam,light,vp));glDrawBuffers(2,color_buffers);
            }
            if (profile_gpu)foliage_queries.end();
            g_engine_perf.forest_gpu_ms=foliage_queries.milliseconds+((early_opaque||authored_first)?early_opaque_queries.milliseconds:0.f);
            latest_color=target.color;
            if(contact_enabled&&graphics.contact_shadows) {
                if (profile_gpu)occlusion_queries.begin(frame);
                WorldOcclusionFrame contact;contact.near_plane=view_near_plane();contact.tan_half_fov=std::tan(render_cam.fov*.5f);
                contact.color=target.color;contact.depth=target.depth;contact.indirect=target.indirect;
                contact.width=target.w;contact.height=target.h;contact.jitter_ndc[0]=2*jitter[0]/target.w;contact.jitter_ndc[1]=2*jitter[1]/target.h;
                contact.sun_view[0]=dot(light.sun,right);contact.sun_view[1]=dot(light.sun,up);contact.sun_view[2]=-dot(light.sun,forward);
                latest_color=world_occlusion_resolve(occlusion,contact);
                if(latest_color==target.color){occlusion_failures++;launch->exit_code=1;running=false;}
                if (profile_gpu)occlusion_queries.end();
            }
            if(profile_gpu)dragon_queries.begin(frame);
            if(dragon_breath){
                const auto breath=dragon_breath_frame(render_cam,light,right,up,forward,vp,latest_color,target.depth,target.w,target.h,visual_time,last_wind_x,last_wind_z);
                if(!world_dragon_breath_draw(dragon_breath,breath)){dragon_breath_failures++;launch->exit_code=1;running=false;}
                const auto bs=world_dragon_breath_stats(dragon_breath);
                dragon_peak_particles=std::max(dragon_peak_particles,bs.flames+bs.cores+bs.smoke+bs.embers);
                dragon_breath_accumulated=bs;dragon_breath_frames++;
            }
            if(profile_gpu)dragon_queries.end();
            if(lightning_ready&&!lightning_visual.draw(authored_storm_output,latest_color,target.depth,target.w,target.h,vp,render_cam.x,render_cam.y,render_cam.z,right,light.sun.y)){++authored_weather_failures;launch->exit_code=1;running=false;}
            weather_view.color=latest_color;weather_view.depth=target.depth;
            if (profile_gpu)weather_queries.begin(frame);
            unsigned weather_color=world_weather_resolve(weather,weather_view);
            if(world_weather_active(weather_view)&&weather_color==latest_color) {
                weather_failures++;launch->exit_code=1;running=false;
            }
            latest_color=weather_color;
            latest_color=world_scene_particles_resolve(scene_particles,scene_particle_frame(render_cam,light,right,up,forward,
                vp,lightVP,target.w,target.h,latest_color,target.depth,shadow.depth,world_environment_effects_profile(graphics.environment_effects),hour,last_wind_x,last_wind_z));
            WorldCombatVfxFrame combat_frame;combat_frame.near_plane=view_near_plane();combat_frame.color=latest_color;combat_frame.depth=target.depth;combat_frame.width=target.w;combat_frame.height=target.h;std::copy_n(vp.m,16,combat_frame.vp);combat_frame.camera={render_cam.x,render_cam.y,render_cam.z};combat_frame.right[0]=right.x;combat_frame.right[1]=right.y;combat_frame.right[2]=right.z;combat_frame.up[0]=up.x;combat_frame.up[1]=up.y;combat_frame.up[2]=up.z;
            if(!world_combat_vfx_draw(combat_vfx_renderer,combat_vfx,combat_frame)){launch->exit_code=1;running=false;}
            if(joust_enabled){
                const auto active=joust_dust.stats().active_particles;joust_dust_peak=std::max(joust_dust_peak,active);
                if(!world_combat_vfx_draw(combat_vfx_renderer,joust_dust,combat_frame)){launch->exit_code=1;running=false;}
                else if(active)++joust_dust_draws;
            }
            if(blood_renderer){
                WorldBloodFrame blood_frame;blood_frame.target=combat_frame;
                blood_frame.sun[0]=light.sun.x;blood_frame.sun[1]=light.sun.y;blood_frame.sun[2]=light.sun.z;
                blood_frame.sun_color[0]=light.sunColor.x;blood_frame.sun_color[1]=light.sunColor.y;blood_frame.sun_color[2]=light.sunColor.z;
                blood_frame.ambient[0]=light.skyLight.x;blood_frame.ambient[1]=light.skyLight.y;blood_frame.ambient[2]=light.skyLight.z;
                if(!world_blood_draw(blood_renderer,blood_effects,blood_frame)){std::fprintf(stderr,"[Blood] Draw failed\n");launch->exit_code=1;running=false;}
            }
            const auto particles=world_scene_particles_stats(scene_particles);
            particle_peak_smoke=std::max(particle_peak_smoke,size_t(particles.smoke_instances));
            particle_peak_dust=std::max(particle_peak_dust,size_t(particles.dust_instances));
            if(particles.failures){launch->exit_code=1;running=false;}
            if (profile_gpu)weather_queries.end();
            if(!joust_enabled&&!authored_cutscene_locked&&!(battlefield_frame.active&&battlefield_frame.cinematic)&&!story_player_dead()&&(!story_civilian_ready||!cam.swimming||skeletal_hero_active)&&((cam.swimming&&combat_motion)||((!weapon_sheathed||arena_player_bank||(skeletal_hero_active&&skeletal_hero.supports_equipment()))&&(fight.engaged()||((playtest_enabled||(first_journey_enabled&&(battlefield_state.sword_owned()||first_journey_outdoors())&&story_sword_unlocked()))&&preview_screen==1&&!destination_loading&&!first_journey_dialogue_active))))&&!third_person&&!cam.fly&&(!arena_bow()||weapon_sheathed)&&!opening_dialogue_visual){
                const V3 sun_camera={dot(light.sun,right),dot(light.sun,up),-dot(light.sun,forward)};
                const V3 world_up_camera={right.y,up.y,-forward.y};
                bool hands_ok=false;
                if(skeletal_hero_active&&skeletal_hero.first_person_ready()){
                    hands_ok=training_weapon.draw_skeletal(latest_color?latest_color:target.color,target.w,target.h,[&](){
                        auto &p=skeletal_first_person;p.use();
                        // Reuse resident scene IBL and the same destination-baked
                        // probes as world actors; no viewmodel-local captures.
                        light_program(p,light,render_cam);bind_local_probes(p,render_cam);
                        const V3 flat_right={std::cos(cam.yaw),0,std::sin(cam.yaw)};
                        const V3 flat_forward={std::sin(cam.yaw),0,-std::cos(cam.yaw)};
                        const M4 local_view=look({0,0,0},{0,std::sin(render_cam.pitch),-std::cos(render_cam.pitch)},{0,1,0});
                        p.m4("uVP",product(perspective(render_cam.fov,render_cam.aspect,.025f,10.f),local_view));
                        p.v3("uOffset",{});p.v2("uWorldAnchor",0,0);p.f("uTime",visual_time);
                        p.v3("uViewWorldOrigin",{float(render_cam.x),float(render_cam.y),float(render_cam.z)});
                        p.v3("uViewWorldRight",flat_right);p.v3("uViewWorldForward",flat_forward);
                        p.v3("uSunDirectionCamera",{dot(light.sun,flat_right),light.sun.y,-dot(light.sun,flat_forward)});
                        p.v3("uSunColor",light.sunColor);p.v3("uSkyFill",light.skyLight);p.v3("uGroundFill",light.groundLight);
                        return skeletal_hero.draw_first_person(p.id,skeletal_hero_error);
                    });
                    if(hands_ok){++skeletal_first_person_draws;if(story_native_swim())++story_swim_first_person_frames;}else ++skeletal_first_person_failures;
                }else hands_ok=training_weapon.draw(arena_player_motion(),latest_color?latest_color:target.color,target.w,target.h,sun_camera,light.sunColor,light.skyLight,light.groundLight,world_up_camera);
                if(!hands_ok){
                    std::fprintf(stderr,"[World] First person arms target failed\n");launch->exit_code=1;running=false;
                }else water_post_hands=cam.swimming;
            }
            if(temporal) {
                if (profile_gpu)temporal_queries.begin(frame);
                WorldTemporalFrame temporal_frame;temporal_frame.near_plane=view_near_plane();
                temporal_frame.color=latest_color;temporal_frame.depth=target.depth;
                temporal_frame.width=target.w;temporal_frame.height=target.h;
                std::memcpy(temporal_frame.view_projection,vp.m,sizeof(vp.m));
                temporal_frame.camera[0]=render_cam.x;temporal_frame.camera[1]=render_cam.y;temporal_frame.camera[2]=render_cam.z;
                temporal_frame.jitter_pixels[0]=jitter[0];temporal_frame.jitter_pixels[1]=jitter[1];
                temporal_frame.reset=temporal_cut||std::fabs(active_storm_light.gain-lightning_history_flash)>.02f||std::fabs(hour-history_hour)>.05f||std::fabs(cloud-history_cloud)>.04f||
                    std::fabs(rain-history_rain)>.05f||std::fabs(wind-history_wind)>.10f||std::fabs(mist-history_mist)>.08f||
                    std::fabs(bearing-history_bearing)>.10f||std::fabs(sun_strength-history_sun_strength)>.02f||std::fabs(sky_fill-history_sky_fill)>.02f;
                lightning_history_flash=active_storm_light.gain;
                temporal_frame.reactive_alpha=true;
                // Player hero object motion: root translation/yaw since the last
                // resolved frame, drawn against the finished scene depth.
                const double hero_root[3]={skeletal_hero_draw?skeletal_hero.x():0.,cam.y-1.75,skeletal_hero_draw?skeletal_hero.z():0.};
                const bool hero_motion_now=skeletal_hero_draw;
                const bool npc_motion_now=world_trail_camp_skeletal(arrival_camp)||battlefield_visible;
                const bool motion_now=object_motion&&(hero_motion_now||npc_motion_now)&&!studio.open;
                if(motion_now&&motion_previous_valid&&!temporal_frame.reset) {
                    const double root_delta[3]={hero_root[0]-motion_previous_root[0],hero_root[1]-motion_previous_root[1],hero_root[2]-motion_previous_root[2]};
                    float yaw_delta=avatar_yaw-motion_previous_yaw;
                    while(yaw_delta>3.14159265f)yaw_delta-=6.28318531f;while(yaw_delta<-3.14159265f)yaw_delta+=6.28318531f;
                    const double travel=std::sqrt(root_delta[0]*root_delta[0]+root_delta[1]*root_delta[1]+root_delta[2]*root_delta[2]);
                    std::string motion_error;
                    if((!hero_motion_now||travel<3.)&&(!hero_motion_now||skeletal_hero.warm_extra(world_object_motion_program(object_motion),motion_error))&&
                       world_object_motion_begin(object_motion,target.w,target.h,target.depth)) {
                        const GLuint mp=world_object_motion_program(object_motion);
                        glUniformMatrix4fv(glGetUniformLocation(mp,"uVP"),1,GL_FALSE,vp.m);
                        glUniformMatrix4fv(glGetUniformLocation(mp,"uPrevVP"),1,GL_FALSE,motion_previous_vp);
                        glUniform3f(glGetUniformLocation(mp,"uOffset"),float(skeletal_hero.x()-render_cam.x),float(-render_cam.y),float(skeletal_hero.z()-render_cam.z));
                        glUniform3f(glGetUniformLocation(mp,"uCameraDelta"),float(render_cam.x-motion_previous_camera[0]),float(render_cam.y-motion_previous_camera[1]),float(render_cam.z-motion_previous_camera[2]));
                        glUniform3f(glGetUniformLocation(mp,"uRootDelta"),float(root_delta[0]),float(root_delta[1]),float(root_delta[2]));
                        glUniform1f(glGetUniformLocation(mp,"uRootYawDelta"),yaw_delta);
                        // Last frame's palette is valid only if exactly one upload
                        // happened since then on the same skinned renderer.
                        const void *identity=nullptr;std::uint64_t uploads=0;
                        int previous_palette_offset=0;const unsigned previous_palette=skeletal_hero.motion_previous_palette(identity,uploads,&previous_palette_offset);
                        const bool skinned=previous_palette&&identity==motion_previous_identity&&uploads==motion_previous_uploads+1;
                        glActiveTexture(GL_TEXTURE18);glBindTexture(GL_TEXTURE_BUFFER,skinned?previous_palette:0);glBindSampler(18,0);glActiveTexture(GL_TEXTURE0);
                        glUniform1i(glGetUniformLocation(mp,"uPrevSkinPalette"),18);glUniform1i(glGetUniformLocation(mp,"uHasPrevPalette"),skinned?1:0);glUniform1i(glGetUniformLocation(mp,"uPrevPaletteOffset"),skinned?previous_palette_offset:0);
                        if(skinned&&hero_motion_now)object_motion_skinned_frames++;
                        bool drawn=!hero_motion_now||skeletal_hero.draw(mp,motion_error);
                        const unsigned npc_drawn=world_trail_camp_draw_motion(arrival_camp,mp,render_cam.x,render_cam.y,render_cam.z,false);
                        npc_object_motion_actors+=npc_drawn;
                        if(battlefield_visible){Program motion_program;motion_program.id=mp;drawn=battlefield_draw(motion_program,render_cam,false,true,false)&&drawn;motion_program.id=0;}
                        // Offscreen campers legitimately submit zero actors; failures are counted by their owner.
                        glActiveTexture(GL_TEXTURE18);glBindTexture(GL_TEXTURE_BUFFER,0);glActiveTexture(GL_TEXTURE0);
                        world_object_motion_end(object_motion);
                        if(drawn){temporal_frame.object_motion=world_object_motion_texture(object_motion);object_motion_frames++;}
                        else if(++object_motion_failures==1)std::fprintf(stderr,"[Motion] Hero motion draw failed: %s\n",motion_error.c_str());
                    } else if((!hero_motion_now||travel<3.)&&++object_motion_failures==1)std::fprintf(stderr,"[Motion] Hero motion unavailable: %s\n",motion_error.c_str());
                }
                if(npc_motion_now&&(!motion_previous_valid||temporal_frame.reset))world_trail_camp_draw_motion(arrival_camp,world_object_motion_program(object_motion),render_cam.x,render_cam.y,render_cam.z,true);
                if(battlefield_visible&&(!motion_previous_valid||temporal_frame.reset)){Program motion_program;motion_program.id=world_object_motion_program(object_motion);battlefield_draw(motion_program,render_cam,false,true,true);motion_program.id=0;}
                std::memcpy(motion_previous_vp,vp.m,sizeof(vp.m));
                motion_previous_camera[0]=render_cam.x;motion_previous_camera[1]=render_cam.y;motion_previous_camera[2]=render_cam.z;
                for(int k=0;k<3;k++)motion_previous_root[k]=hero_root[k];
                motion_previous_yaw=avatar_yaw;motion_previous_valid=motion_now;
                {std::uint64_t uploads=0;skeletal_hero.motion_previous_palette(motion_previous_identity,uploads);motion_previous_uploads=uploads;}
                world_temporal_resolve(temporal,temporal_frame);
                if(upscaler){
                    upscale_frame=WorldUpscaleFrame{};
                    upscale_frame.raw=temporal_frame.color;upscale_frame.stable=world_temporal_output(temporal,0);
                    upscale_frame.depth=world_temporal_depth(temporal,0);upscale_frame.object_motion=temporal_frame.object_motion;
                    upscale_frame.in_width=target.w;upscale_frame.in_height=target.h;
                    std::memcpy(upscale_frame.view_projection,temporal_frame.view_projection,sizeof(upscale_frame.view_projection));
                    for(int k=0;k<3;++k)upscale_frame.camera[k]=temporal_frame.camera[k];
                    upscale_frame.jitter_pixels[0]=temporal_frame.jitter_pixels[0];upscale_frame.jitter_pixels[1]=temporal_frame.jitter_pixels[1];
                    upscale_frame.reset=temporal_frame.reset;
                    upscale_frame_id=frame;
                }
                if (profile_gpu)temporal_queries.end();
                temporal_cut=false;temporal_sample++;
                previous_rendered_visual_time=visual_time;
                history_hour=hour;history_cloud=cloud;history_rain=rain;history_wind=wind;history_mist=mist;
                history_bearing=bearing;history_sun_strength=sun_strength;history_sky_fill=sky_fill;
            }
        }

        const auto current_flames=world_torch_flames_stats(torch_flames);
        if(near_bandit_camp&&!qa_bandit_lights_off) {
            bandit_flame_draws+=current_flames.draws-last_torch_draws;
            bandit_peak_flames=std::max(bandit_peak_flames,size_t(current_flames.visible_emitters));
        }
        last_torch_draws=current_flames.draws;
        if(ambience){
            WorldAmbienceFrame af;af.dt=launch->frames?1.f/60.f:dt;af.listener[0]=cam.x;af.listener[1]=cam.y;af.listener[2]=cam.z;af.yaw=cam.yaw;
            af.hearth=ambience_hearth_ready&&(arrival_camp_enabled||std::hypot(cam.x-ambience_hearth[0],cam.z-ambience_hearth[2])<60.);
            for(int k=0;k<3;++k)af.hearth_pos[k]=ambience_hearth[k];
            af.hour=hour;af.foot_plants=skeletal_hero.stats().foot_plants;af.hero_grounded=!cam.swimming;
            af.hero_pos[0]=skeletal_hero.x();af.hero_pos[1]=cam.y;af.hero_pos[2]=skeletal_hero.z();
            int mv=100,muv=100,sv=100;audio_get_volumes(&mv,&muv,&sv);af.sfx_volume=float(mv)*float(sv)/10000.f;
            af.indoor=opening_enabled&&(opening_state.stage()<=world_opening_prologue::Stage::Transport||opening_state.stage()==world_opening_prologue::Stage::GameMenu);
            {
                const auto window_flags=SDL_GetWindowFlags(window);
                af.focused=!(window_flags&SDL_WINDOW_MINIMIZED)&&(launch->frames||launch->benchmark||(window_flags&SDL_WINDOW_INPUT_FOCUS));
            }
            af.paused=authored_paused;af.loading=destination_loading;
            af.hero_steps=demo_audio==nullptr;   // WorldDemoAudio owns hero footsteps when present
            world_ambience_update(ambience,af);
        }
        if (profile_gpu)post_queries.begin(frame);
        const GLuint scene=world_temporal_output(temporal,latest_color?latest_color:target.color);
        if(auto_exposure)auto_exposure_factor=world_auto_exposure_update(auto_exposure,scene,target.w,target.h,launch->frames?1.f/60.f:dt);
        const GLuint glow=bloom_enabled?world_bloom_resolve(bloom,scene,target.w,target.h,exposure*auto_exposure_factor):0;
        if(bloom_enabled&&!glow){bloom_failures++;launch->exit_code=1;running=false;}
        if(glow)bloom_play_frames++;
        GLuint post_scene=scene;post_scene_w=post_scene_h=0;
        if(upscaler&&upscale_frame_id==frame&&!studio.open&&upscale_frame.stable==scene&&
           designer_drawable.w>target.w+2&&designer_drawable.h>target.h+2) {
            upscale_frame.out_width=designer_drawable.w;upscale_frame.out_height=designer_drawable.h;
            if(const GLuint hi=world_upscale_resolve(upscaler,upscale_frame)){post_scene=hi;post_scene_w=designer_drawable.w;post_scene_h=designer_drawable.h;}
        }
        compose_post(post_scene,world_temporal_depth(temporal,target.depth),glow);
        post_scene_w=post_scene_h=0;
        if (profile_gpu)post_queries.end();
        glDepthMask(GL_TRUE);
        if(!launch->profile[0]) {
            foliage_sequence.capture(frame,launch->frames,display_w,display_h);
            npc_sequence.capture(frame,launch->frames,display_w,display_h);
        }
        WorldCoverMetrics foliage_cycle_metrics={};
        if(foliage_cycle_dir){
            world_cover_metrics(dressing.cover,&foliage_cycle_metrics);
            const auto budget=world_ground_cover_quality::profile(graphics.foliage_density);
            const bool settled=!graphics_apply&&!foliage_cycle_advance&&!destination_loading&&view_ready&&
                !foliage_cycle_metrics.near_building&&!foliage_cycle_metrics.tree_building&&
                dressing.fields[WORLD_COVER_GRASS].count<=budget.grass&&dressing.fields[WORLD_COVER_FERNS].count<=budget.ferns;
            foliage_cycle_settled=settled?foliage_cycle_settled+1:0;
        }
        if(foliage_cycle_dir&&foliage_cycle_settled>=60&&foliage_cycle_phase<5){
            const auto budget=world_ground_cover_quality::profile(graphics.foliage_density);
            const auto &gf=dressing.fields[WORLD_COVER_GRASS],&ff=dressing.fields[WORLD_COVER_FERNS];
            const std::string prefix=std::string(foliage_cycle_dir)+"/"+world_foliage_density_name(graphics.foliage_density)+"-frame"+std::to_string(frame);
            std::vector<unsigned char> pixels(size_t(display_w)*display_h*3);
            GLint alignment=4;glGetIntegerv(GL_PACK_ALIGNMENT,&alignment);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadBuffer(GL_BACK);
            glReadPixels(0,0,display_w,display_h,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());glPixelStorei(GL_PACK_ALIGNMENT,alignment);
            const size_t stride=size_t(display_w)*3;
            for(int row=0;row<display_h/2;++row)std::swap_ranges(pixels.data()+row*stride,pixels.data()+(row+1)*stride,pixels.data()+(display_h-1-row)*stride);
            SDL_Surface *surface=SDL_CreateRGBSurfaceWithFormatFrom(pixels.data(),display_w,display_h,24,int(stride),SDL_PIXELFORMAT_RGB24);
            const bool saved=surface&&SDL_SaveBMP(surface,(prefix+".bmp").c_str())==0;if(surface)SDL_FreeSurface(surface);
            const unsigned error=glGetError();
            WorldCoverMetrics cm={};world_cover_metrics(dressing.cover,&cm);
            std::ofstream report(std::string(foliage_cycle_dir)+"/tiers.jsonl",std::ios::app);
            report<<"{\"tier\":\""<<world_foliage_density_name(graphics.foliage_density)<<"\",\"frame\":"<<frame<<",\"grass_budget\":"<<budget.grass<<",\"fern_budget\":"<<budget.ferns<<",\"grass_field\":"<<gf.count<<",\"fern_field\":"<<ff.count<<",\"scanned_grass\":"<<dressing.scanned_grass_count<<",\"short_grass\":"<<dressing.grass.blades<<",\"tree_field\":"<<dressing.fields[WORLD_COVER_TREES].count<<",\"rock_field\":"<<dressing.fields[WORLD_COVER_ROCKS].count<<",\"staging\":"<<(cm.near_building?"true":"false")<<",\"applies\":"<<graphics_apply_count<<",\"saved\":"<<(saved?"true":"false")<<",\"gl_error\":"<<error<<"}\n";
            if(!saved||error||!report||dressing.selected_budget_tier!=int(graphics.foliage_density)||int(graphics.foliage_density)!=int(foliage_cycle_phase%4)){launch->exit_code=1;running=false;}
            ++foliage_cycle_phase;foliage_cycle_settled=0;
            if(foliage_cycle_phase<5)foliage_cycle_advance=true;else running=false;
        }
        if(qa_grass&&view_ready&&!destination_loading)grass_interaction_capture(display_w,display_h);
        if(source_review.active&&view_ready&&!destination_loading&&!source_review_waiting_frame)
            if(!capture_source_review(source_review,display_w,display_h)){launch->exit_code=1;running=false;}
        adaptive_queries.end(g_engine_perf.raw_update_ms); // Update-only CPU work excludes draw-driver/present waits.
        if (profile_gpu)
            queries.end();
        // Submit the completed scene before the UI backend reads/restores GL
        // state. This lets queued scene work begin while CPU overlay work runs,
        // rather than relying on a later state query or swap to dispatch it.
        // glFlush submits asynchronously; it never waits for GPU completion.

        glFlush();early_gpu_submitted_frames++;
        if(profile_gpu&&view_ready&&frame>uint64_t(launch->warmup)) {
            sky_gpu_sum+=sky_queries.milliseconds+(reflection_rendered?sky_reflection_queries.milliseconds:0.f);
            occlusion_gpu_sum+=(contact_enabled&&graphics.contact_shadows)?occlusion_queries.milliseconds:0.f;
            weather_gpu_sum+=weather_queries.milliseconds;dragon_gpu_sum+=dragon_queries.milliseconds;lighting_gpu_samples++;
            terrain_gpu_sum+=terrain_queries.milliseconds;water_gpu_sum+=water_queries.milliseconds;
            temporal_gpu_sum+=temporal_queries.milliseconds;post_gpu_sum+=post_queries.milliseconds;
        }
        g_engine_perf.render_total_gpu_ms = queries.milliseconds;
        g_engine_perf.terrain_gpu_ms=terrain_queries.milliseconds;g_engine_perf.postfx_gpu_ms=post_queries.milliseconds;
        g_engine_perf.raw_render_cpu_ms = millis(render_start);
        g_engine_perf.render_ms = g_engine_perf.raw_render_cpu_ms;
        Uint64 ui_begin = SDL_GetPerformanceCounter();
        #include "engine/world_editor_placement_qa.inc"
        const bool ui_loading=destination_loading||!terrain.view_ready(cam.x,cam.z,cam.fly);
        // Gameplay HUDs still render and drain queued ImGui events, but do not
        // poll global mouse/cursor/gamepad APIs. Those synchronous desktop
        // round trips can stall Wine. Menus and interactive panels retain the
        // full SDL backend for ordinary mouse, keyboard and controller input.
        if((presentation&&preview_menu_input)||studio.open||studio.designer_restore_selection_after_play||show_tools||show_graphics||show_map||ui_loading||(studio.components_runtime&&studio.components_runtime->current_line()))editor_ui_begin_frame(&ui);
        else editor_ui_begin_passive_frame(&ui,dt,display_w,display_h);
        if (ui_loading&&!presentation) {
            ImGui::SetNextWindowPos({float(display_w) * .5f, float(display_h) * .5f}, ImGuiCond_Always, {.5f, .5f});
            ImGui::Begin("Preparing destination", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove);
            ImGui::Text("Streaming %s...", current_name);
            ImGui::TextUnformatted("The previous view stays visible until the terrain is ready.");
            ImGui::End();
        }
        if(studio.designer_restore_selection_after_play&&!studio.open&&studio.production_playing()){
            ImGui::SetNextWindowPos({float(display_w)-18,18},ImGuiCond_Always,{1,0});ImGui::SetNextWindowBgAlpha(.72f);
            if(ImGui::Begin("Editor playtest",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoSavedSettings)){
                if(ImGui::Button("Return to Editor  F7")){if(studio.editor_session->request_stop())studio.production_stop(world_editor::StopChoice::DiscardPlayChanges);look_active=false;SDL_SetRelativeMouseMode(SDL_FALSE);}
            }ImGui::End();
        }

        #include "engine/world_studio_runtime_qa.inc"
        #include "engine/world_studio_production_qa.inc"
        #include "engine/world_studio_designer_qa.inc"
        #include "engine/world_editor_copy_paste_qa.inc"
        #include "engine/world_editor_terrain_snap_qa.inc"
        #include "engine/world_editor_vfx_qa.inc"
        #include "engine/world_editor_landscape_qa.inc"
        #include "engine/world_editor_landscape_refine_qa.inc"
        #include "engine/world_editor_water_refine_qa.inc"
        #include "engine/world_editor_zoom_qa.inc"
        #include "engine/world_editor_reliability_qa.inc"
        #include "engine/world_editor_item_wind_qa.inc"
        #include "engine/world_editor_structure_qa.inc"
        #include "engine/world_editor_npc_paths_qa.inc"
        #include "engine/world_editor_completion_qa.inc"
        #include "engine/world_editor_optimization_qa.inc"
        #include "engine/world_editor_workflows_v17_qa.inc"
        if(qa_studio&&!destination_loading&&terrain.view_ready(cam.x,cam.z,cam.fly)){
            if(qa_studio_phase==0&&frame>30){
                studio.open=true;studio.checkpoint();
                qa_studio_x=cam.x+55;qa_studio_z=cam.z+25;
                if(!world_studio::brush(studio.draft,qa_studio_x,qa_studio_z,6,1,0,0,world_atlas_base_height,studio.message)){launch->exit_code=1;running=false;}
                qa_studio_expected=world_atlas_base_height(qa_studio_x,qa_studio_z)+world_studio::height_delta(studio.draft,qa_studio_x,qa_studio_z);
                studio.draft.stamps.push_back({qa_studio_x,qa_studio_z,12,0,1});
                for(size_t i=0;i<studio.source.size();i++){const auto &p=studio.source[i];if(p.tree&&std::hypot(p.x-qa_studio_x,p.z-qa_studio_z)<70){studio.draft.overrides.push_back({unsigned(i),p.x,p.z,std::clamp(float(p.yaw*180/3.141592653589793),-360.f,360.f),p.scale,true});break;}}
                world_studio::Placement crate;crate.alias="starting_crate";crate.x=qa_studio_x+12;crate.z=qa_studio_z;studio.draft.placements.push_back(crate);
                world_studio::Light point;point.x=cam.x+3;point.z=cam.z-3;point.y=cam.y+1;point.power=18;point.radius=10;studio.draft.lights.push_back(point);
                point.x+=2;point.cone=60;point.pitch=-70;studio.draft.lights.push_back(point);
                world_studio::Material material;material.alias="starting_crate";material.normal=.7f;material.tint={.9f,.65f,.4f};studio.draft.materials.push_back(material);
                world_studio::Decal patch;patch.x=cam.x;patch.z=cam.z-5;patch.y=world_atlas_height(patch.x,patch.z);patch.size={4,2,4};studio.draft.decals.push_back(patch);
                world_studio::Spline path;path.alias="bep_prop_fence";path.points={{{qa_studio_x+20,qa_studio_z}},{{qa_studio_x+26,qa_studio_z+6}},{{qa_studio_x+33,qa_studio_z+5}}};path.kind=1;path.spacing=4;
                std::vector<world_studio::Placement> instances;if(!world_studio::build_spline(path,instances,studio.message)){launch->exit_code=1;running=false;}
                studio.draft.splines.push_back(path);studio.draft.placements.insert(studio.draft.placements.end(),instances.begin(),instances.end());
                if(qa_studio_tools){
                    auto &crate=studio.draft.placements.front();crate.x=cam.x+5;crate.z=cam.z-10;crate.offset_y=.25f;
                    studio.draft.groups.push_back({"QA grouped props",{8192,8193}});
                    if(const char *stem=std::getenv("VALWOOD_QA_STUDIO_IMPORT_STEM")){world_studio::Asset a;a.alias="qa_studio_asset";a.stem=stem;a.lods=2;studio.draft.assets.push_back(a);auto copy=crate;copy.alias=a.alias;copy.x+=3;studio.draft.placements.push_back(copy);}
                    if(const char *path=std::getenv("VALWOOD_QA_STUDIO_ENVIRONMENT")){auto &e=studio.draft.environment;e.path=path;e.enabled=true;e.rotation=35;e.intensity=1;e.strength=.6;}
                    studio.choose(8192,false,false);studio.group_pick=false;studio.requested_tab=2;
                }
                studio.changed(true);studio.apply=true;qa_studio_phase=1;
            }else if(qa_studio_phase==1&&studio.applied==1&&frame>150){
                if(std::abs(world_atlas_height(qa_studio_x,qa_studio_z)-qa_studio_expected)>.001f||world_studio::vegetation(studio.live,1,qa_studio_x,qa_studio_z)!=0){launch->exit_code=1;running=false;}
                size_t hidden=0;for(const auto &o:studio.live.overrides)if(o.hidden)hidden++;
                if(world_scenery_stats(scenery).source_instances!=studio.source.size()+studio.live.placements.size()-hidden){launch->exit_code=1;running=false;}
                studio.checkpoint();if(qa_studio_tools&&!studio.draft.assets.empty())studio.draft.assets[0].stem="imports/missing_qa_revision/asset";else{auto bad=studio.draft.placements.front();bad.alias="missing_test_model";studio.draft.placements.push_back(bad);}studio.changed(true);studio.apply=true;qa_studio_phase=2;
            }else if(qa_studio_phase==2&&studio.rejected==1){
                if(studio.live.lights.size()!=2||std::abs(world_atlas_height(qa_studio_x,qa_studio_z)-qa_studio_expected)>.001f||!studio.history.undo(studio.draft)){launch->exit_code=1;running=false;}
                studio.changed(true);studio.apply=true;qa_studio_phase=3;
            }else if(qa_studio_phase==3&&studio.applied==2){
                studio.reload_shaders=true;studio.fail_shader_test=true;studio.apply=true;qa_studio_phase=4;
            }else if(qa_studio_phase==4&&studio.rejected==2){
                world_studio::Document check;std::string error;
                if(!studio.save()||!world_studio::load(studio.path.c_str(),check,error)||check.lights.size()!=2||check.placements.size()!=studio.live.placements.size()){
                    std::fprintf(stderr,"[Studio QA] persistence failure: %s\n",error.c_str());launch->exit_code=1;running=false;
                }
                qa_studio_phase=5;studio.message="QA passed: terrain/collision height, foliage mask, placed models, point/spot lights, material, spline, patch, rejected edit, undo, save/reload.";
                std::printf("[Studio QA] PASS:2 applied commits, rejected scene + rejected shader, undo, transactional save/reload and shared terrain height %.4f\n",qa_studio_expected);
            }
        }
        int destination = -1, scenic_destination = -1;
        bool editor_quickspot_jump=false;
        const bool ui_was_flying=cam.fly;
        ImGui::BeginDisabled(launch->benchmark);
        if ((!studio.open&&show_tools)||show_map)
            destination = world_panel(cam, terrain, materials[7], atlas_map_attempted, show_tools&&!studio.open, show_map, show_graphics, hour, cloud, rain, wind, mist,
                bearing,sun_strength,sky_fill,exposure,grade_strength,fps,running,current_name,scenic_destination,population);
        ImGui::EndDisabled();
        studio.designer_asset_thumbnails.begin_frame(studio.project_directory,std::uint64_t(ImGui::GetFrameCount()));
        studio.designer_asset_ghost_vp=product(perspective(render_cam.fov,render_cam.aspect,view_near_plane(),15000.f),view);
        studio.designer_asset_ghost_eye={0,0,0};studio.designer_asset_ghost_near=view_near_plane();
        studio.designer_asset_ghost_depth=(!ui_loading&&view_ready&&!destination_loading)?world_temporal_depth(temporal,target.depth):0;
        studio.designer_asset_ghost_rect[0]=designer_drawable.x;studio.designer_asset_ghost_rect[1]=designer_drawable.y;
        studio.designer_asset_ghost_rect[2]=designer_drawable.w;studio.designer_asset_ghost_rect[3]=designer_drawable.h;
        studio.refresh_scene(scenery);studio.project_streaming=world_scenery_streaming_stats(scenery);
        ImGui::BeginDisabled(authored_cutscene_locked);studio.panel(render_cam,fps,terrain.bytes(),show_graphics);ImGui::EndDisabled();
        if(qa_quickspot_stream){
            static constexpr const char *spots[]={"seldonne-arrival","castle-village","apartment"};
            if(qa_quickspot_waiting&&!destination_loading&&view_ready){
                const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-qa_quickspot_started).count();
                const auto stream=world_scenery_streaming_stats(scenery);
                std::fprintf(stderr,"[Quick Spots QA] READY %s %.1f ms; critical=%zu resident=%zu textures=%.1f MiB\n",spots[qa_quickspot_step],ms,stream.critical_pending,stream.resident,stream.texture_bytes/1048576.);
                qa_quickspot_waiting=false;++qa_quickspot_step;
            }
            if(!qa_quickspot_waiting&&qa_quickspot_step<std::size(spots)&&frame>90&&view_ready&&!destination_loading){
                for(unsigned i=0;i<std::size(scenic_views);++i)if(!std::strcmp(spots[qa_quickspot_step],scenic_views[i].id)){
                    studio.quickspot_destination=int(i);qa_quickspot_started=std::chrono::steady_clock::now();qa_quickspot_waiting=true;
                    std::fprintf(stderr,"[Quick Spots QA] JUMP %s\n",spots[qa_quickspot_step]);break;
                }
            }
            if(qa_quickspot_step==std::size(spots))running=false;
        }
        if(qa_terrain_apply&&frame>60&&view_ready&&!destination_loading){
            if(qa_terrain_apply_phase==0){
                studio.radius=7;bool found=false;
                for(const auto &p:std::array<std::array<double,2>,6>{{{{-11840,5310}},{{-11780,5280}},{{-11680,5340}},{{-11850,5480}},{{-11400,5460}},{{-11500,5280}}}}){
                    studio.x=p[0];studio.z=p[1];if(studio.safe_brush()){found=true;break;}
                }
                if(!found){std::fprintf(stderr,"[Terrain Apply QA] FAIL no safe sculpt location\n");launch->exit_code=1;running=false;}
                else{
                    auto next=studio.draft;std::string error;
                    if(!world_studio::brush(next,studio.x,studio.z,studio.radius,.6f,0,0,world_atlas_base_height,error)){
                        std::fprintf(stderr,"[Terrain Apply QA] FAIL brush: %s\n",error.c_str());launch->exit_code=1;running=false;
                    }else{studio.checkpoint();studio.draft=std::move(next);qa_terrain_apply_before=studio.applied;
                        studio.changed(true);studio.apply=true;qa_terrain_apply_started=std::chrono::steady_clock::now();qa_terrain_apply_phase=1;}
                }
            }else if(qa_terrain_apply_phase==1&&studio.applied>qa_terrain_apply_before){
                const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-qa_terrain_apply_started).count();
                std::fprintf(stderr,"[Terrain Apply QA] PASS commit %.1f ms, field/scene total %.1f ms, point %.1f %.1f\n",ms,studio.last_apply_ms,studio.x,studio.z);
                qa_terrain_apply_phase=2;running=false;
            }
        }
        if(qa_editor_move&&view_ready&&!ui_loading&&!destination_loading){
            if(qa_editor_move_phase==0&&frame>30){
                bool found=false;for(unsigned key=0;key<studio.source.size();key++)if(studio.source_poses[key].alias.rfind("nm_opening_reference_item_",0)==0&&studio.source_explicit_pivots[key]&&key<studio.applied_records.size()&&studio.applied_records[key]!=SIZE_MAX){qa_editor_move_key=key;found=true;break;}
                if(!found){std::fprintf(stderr,"[Editor move QA] no editable elevated apartment item\n");launch->exit_code=1;running=false;}
                else{
                    studio.choose(qa_editor_move_key,false);
                    const auto baseline=studio.draft;
                    studio.drag.active=true;studio.drag.keys={qa_editor_move_key};studio.drag.before=baseline;
                    std::array<double,32> preview_ms{};bool preview_ok=true;
                    for(unsigned i=0;i<preview_ms.size();i++){
                        const auto started=std::chrono::steady_clock::now();auto next=baseline;std::string preview_error;
                        if(!world_studio::preview_transform_selection(next,studio.source_poses,studio.drag.keys,studio.source_poses[qa_editor_move_key].root,{.01*double(i+1),0,0},0,1,preview_error)){preview_ok=false;break;}
                        studio.draft=std::move(next);studio.revision++;studio.sync_object_preview(scenery);
                        preview_ms[i]=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();
                    }
                    studio.clear_object_preview(scenery);studio.draft=baseline;studio.drag.active=false;
                    std::sort(preview_ms.begin(),preview_ms.end());
                    std::fprintf(stderr,"[Editor move QA] 32 apartment drag previews: p95 %.3fms max %.3fms\n",preview_ms[30],preview_ms[31]);
                    if(!preview_ok){std::fprintf(stderr,"[Editor move QA] drag preview failed\n");launch->exit_code=1;running=false;}
                    else{
                        studio.history.checkpoint(studio.draft);
                        std::string error;
                        if(!world_studio::transform_selection(studio.draft,studio.source_poses,{qa_editor_move_key},studio.source_poses[qa_editor_move_key].root,{.35,0,0},0,1,error)||!studio.pose(qa_editor_move_key,qa_editor_move_target)){
                            std::fprintf(stderr,"[Editor move QA] transform failed: %s\n",error.c_str());launch->exit_code=1;running=false;
                        }else{
                            qa_editor_move_start=studio.fast_move_commits;qa_editor_move_applied=studio.applied;studio.changed(true);studio.designer_move_fast_candidate=true;studio.designer_preview_object_edit();qa_editor_move_phase=1;
                        }
                    }
                }
            }else if(qa_editor_move_phase==1&&studio.applied>qa_editor_move_applied&&studio.fast_move_commits==qa_editor_move_start){
                std::fprintf(stderr,"[Editor move QA] slow scenery rebuild handled the apartment move\n");launch->exit_code=1;running=false;
            }else if(qa_editor_move_phase==1&&studio.fast_move_commits>qa_editor_move_start){
                WorldSceneryPlacement actual{};const auto record=studio.applied_records[qa_editor_move_key];
                if(!world_scenery_placement(scenery,record,&actual)||std::abs(actual.x-qa_editor_move_target.root.x)>.002||std::abs(actual.y-qa_editor_move_target.root.y)>.002){
                    std::fprintf(stderr,"[Editor move QA] committed pose differs from draft\n");launch->exit_code=1;
                }else std::fprintf(stderr,"[Editor move QA] PASS elevated furniture committed in place in %.3fms, no scene reload\n",studio.last_apply_ms);
                qa_editor_move_phase=2;running=false;
            }
        }
        if(studio.quickspot_destination>=0){
            if(!studio.production_freeze()&&!studio.cinematic_preview){
                scenic_destination=studio.quickspot_destination;
                editor_quickspot_jump=true;
                studio.message=std::string("Viewing ")+scenic_views[scenic_destination].name+". Nearby detail continues streaming; draft edits are unchanged.";
            }else studio.message="Stop scene Play or the camera timeline before jumping to a Quick Spot.";
            studio.quickspot_destination=-1;
        }
        if(!studio.drag.active)studio.designer_asset_thumbnails.service();
        const auto browser_qa_result=browser_qa.tick(studio,render_cam,frame,launch->editor&&launch->frames&&!launch->benchmark,view_ready&&!ui_loading&&!destination_loading);
        if(browser_qa_result==AssetBrowserQA::Result::Passed)running=false;
        if(browser_qa_result==AssetBrowserQA::Result::Failed){launch->exit_code=1;running=false;}
        const auto long_session_result=long_session_qa.tick(studio,cam,render_cam,dressing,terrain,scenery,window,frame,
            qa_editor_long_session,view_ready&&!ui_loading&&!destination_loading);
        if(long_session_result==EditorLongSessionQA::Result::Passed)running=false;
        if(long_session_result==EditorLongSessionQA::Result::Failed){launch->exit_code=1;running=false;}
        const auto structure_spline_result=structure_spline_qa.tick(studio,cam,scenery,window,frame,qa_structure_splines,view_ready&&!ui_loading&&!destination_loading,structure_gpu,active_playtest_walk,structure_live_fingerprint,bool(structure_ready));
        if(structure_spline_result==EditorStructureSplineQA::Result::Passed)running=false;
        if(structure_spline_result==EditorStructureSplineQA::Result::Failed){launch->exit_code=1;running=false;}
        studio.production_service(!launch->frames&&!launch->benchmark&&!ui_loading&&!studio.drag.active&&!studio.landscape_painting&&!ImGui::IsAnyItemActive());
        if((studio.production_freeze()||studio.cinematic_preview)&&look_active){look_active=false;fight.set_guard(false);SDL_SetRelativeMouseMode(SDL_FALSE);}
        if(!authored_cutscene_locked)studio.components_dialogue();
        if(!presentation&&!authored_cutscene_locked)draw_journey();
        cutscene_draw();
        if(cam.fly!=ui_was_flying)temporal_cut=true;
        if(graphics_panel(window,show_graphics,graphics_draft,graphics,quality,fps,display_w,display_h,target.w,target.h,graphics_message,resident_character_textures,resident_environment_textures,resident_texture_pool_bytes)) {
            if(world_graphics_settings_valid(graphics_draft)){
                graphics_pending=graphics_draft;graphics_apply=true;
                std::snprintf(graphics_message,sizeof(graphics_message),"Preparing selected quality...");
            }
        }
        double combat_ox,combat_oz;world_atlas_village_origin(&combat_ox,&combat_oz);
        if(joust_enabled)joust_ui(render_cam,ui_loading);
        else if(!authored_cutscene_locked)preview_ui(render_cam,ui_loading);
        if(!joust_enabled)inventory_ui(ui_loading);
        if(presentation&&!launch->no_hud&&!ui_loading&&!studio.open&&!show_graphics&&!inventory_open&&preview_screen==1&&
           (!arena_enabled||arena_ui_phase==ArenaUiPhase::Fight)&&!fight_paused)draw_combat_feedback(combat_feedback);
        #include "engine/world_npc_nameplates.inc"
        world_minimap::Visibility minimap_visibility;
        minimap_visibility.hud=!joust_enabled&&!launch->no_hud;minimap_visibility.editor=studio.open;
        minimap_visibility.modal=preview_blocked()||milestone_review.active()||show_tools;
        minimap_visibility.dialogue=studio.components_runtime&&studio.components_runtime->current_line();
        minimap_visibility.atlas=show_map;minimap_visibility.graphics=show_graphics;
        minimap_visibility.loading=ui_loading||!view_ready||destination_loading;
        minimap_visibility.cinematic=authored_cutscene_locked||studio.cinematic_preview||studio.production_freeze()||opening_camera_locked;
        minimap_visibility.flying=cam.fly;
        minimap_drawn=minimap_valid&&logical_w>=400&&logical_h>=270&&world_minimap::visible(minimap_config,{cam.x,cam.z},minimap_visibility);
        if(minimap_drawn){
            if(minimap_revision!=studio.applied||minimap_ribbons!=studio.live.surfaces.ribbons.data()||minimap_structures!=studio.live.structures.data()){
                minimap_geometry.clear();++minimap_stats.cache_builds;
                const unsigned paths=unsigned(studio.live.surfaces.ribbons.size()+studio.live.structures.size());
                const unsigned quota=std::min(64u,world_minimap::line_limit/std::max(1u,paths));
                // Equal per-path budgets preserve every river/road and bridge,
                // including short paths after the long perimeter rivers.
                for(bool water:{true,false})for(const auto &r:studio.live.surfaces.ribbons){
                    if((r.kind==world_surface::RibbonKind::River)!=water)continue;
                    world_surface::BakedRibbon baked;std::string error;
                    if(!world_surface::bake_ribbon(r,baked,error)||baked.samples.size()<2){++minimap_stats.cache_failures;continue;}
                    const unsigned last=unsigned(baked.samples.size()-1),count=std::min(quota,last);
                    for(unsigned j=0;j<count;++j){const auto &a=baked.samples[j*last/count].position,&b=baked.samples[(j+1)*last/count].position;
                        minimap_geometry.append({a.x,a.z},{b.x,b.z},r.width,water?world_minimap::LineKind::River:world_minimap::LineKind::Road);}
                }
                for(const auto &d:studio.live.structures)if(d.kind==world_structure::Kind::Bridge){
                    world_structure::Generated generated;std::string error;if(!world_structure::build(d,generated,error)||generated.segments.empty()){++minimap_stats.cache_failures;continue;}
                    const unsigned last=unsigned(generated.segments.size()),count=std::min(quota,last);
                    for(unsigned j=0;j<count;++j){const auto &a=generated.segments[j*last/count].a,&b=generated.segments[(j+1)*last/count-1].b;
                        minimap_geometry.append({a.x,a.z},{b.x,b.z},d.width,world_minimap::LineKind::Bridge);}
                }
                minimap_revision=studio.applied;minimap_ribbons=studio.live.surfaces.ribbons.data();minimap_structures=studio.live.structures.data();
            }
            std::array<world_minimap::Marker,32> dots{};unsigned dots_count=0;
            for(unsigned i=0;i<world_village_resident_count+WORLD_EDITED_ACTOR_CAPACITY&&dots_count<dots.size();++i){WorldVillageResident resident;
                if(world_village_population_resident(population,i,&resident)&&std::abs(resident.x-cam.x)<=minimap_config.radius&&std::abs(resident.z-cam.z)<=minimap_config.radius)dots[dots_count++]={{resident.x,resident.z},world_minimap::MarkerKind::Walker};}
            for(unsigned i=0;i<world_road_traffic_count(road_traffic)&&dots_count<dots.size();++i){WorldRoadTrafficActor a;if(world_road_traffic_actor(road_traffic,i,&a)&&a.active&&std::abs(a.x-cam.x)<=minimap_config.radius&&std::abs(a.z-cam.z)<=minimap_config.radius)dots[dots_count++]={{a.x,a.z},a.kind==WorldRoadTrafficKind::Rider?world_minimap::MarkerKind::Rider:a.kind==WorldRoadTrafficKind::Pullcart?world_minimap::MarkerKind::Cart:world_minimap::MarkerKind::Walker};}
            draw_corner_minimap(minimap_config,minimap_geometry,cam.x,cam.y-1.75,cam.z,cam.yaw,float(logical_w),float(logical_h),dots,dots_count,minimap_stats);
            ++minimap_stats.visible_frames;
        }else ++minimap_stats.hidden_frames;
        if(!playtest_enabled&&!opening_enabled&&!launch->no_hud&&!studio.open&&!minimap_visibility.modal&&!minimap_visibility.dialogue&&!minimap_visibility.atlas&&!minimap_visibility.graphics&&!minimap_visibility.loading&&!minimap_visibility.cinematic){ImGui::SetNextWindowPos({float(logical_w)-16,minimap_drawn?minimap_stats.bottom+8:16},ImGuiCond_Always,{1,0});ImGui::SetNextWindowBgAlpha(.45f);ImGui::Begin("Camera mode",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoInputs|ImGuiWindowFlags_NoSavedSettings);ImGui::Text("%s%s | V: switch view",cam.swimming&&!cam.fly?(cam.dive.diving?"Diving | ":"Swimming | "):"",third_person?"Third person":"First person");if(cam.swimming&&!cam.fly)ImGui::TextUnformatted(preview_pad.using_pad?"LT: dive | RT: rise | Left stick: swim":"Ctrl / C: dive | Space: rise | WASD: swim");ImGui::End();}
        if(!presentation&&!studio.open)training_panel(fight,fight.near(cam.x-combat_ox,cam.y,cam.z-combat_oz,cam.fly),!cam.fly&&std::hypot(cam.x-combat_ox-fight.instructor_x,cam.z-combat_oz-fight.instructor_z)<14.,fight_paused,display_w,display_h,!third_person);
        editor_ui_render(&ui);
        minimap_capture();
        walking_controls_capture();
        // Preserve the actual failed test frame after ImGui renders, so a
        // bounded disposable editor replay remains visually diagnosable.
        if((qa_landscape_refine||qa_structure)&&!running&&launch->exit_code&&launch->capture[0]&&display_w>0&&display_h>0){
            std::vector<unsigned char> pixels(std::size_t(display_w)*display_h*3);
            GLint alignment=4;glGetIntegerv(GL_PACK_ALIGNMENT,&alignment);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadBuffer(GL_BACK);
            glReadPixels(0,0,display_w,display_h,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());glPixelStorei(GL_PACK_ALIGNMENT,alignment);
            const std::size_t stride=std::size_t(display_w)*3;
            for(int row=0;row<display_h/2;++row)std::swap_ranges(pixels.data()+row*stride,pixels.data()+(row+1)*stride,pixels.data()+(display_h-1-row)*stride);
            SDL_Surface *surface=SDL_CreateRGBSurfaceWithFormatFrom(pixels.data(),display_w,display_h,24,int(stride),SDL_PIXELFORMAT_RGB24);
            const bool captured=surface&&SDL_SaveBMP(surface,launch->capture)==0;if(surface)SDL_FreeSurface(surface);
            std::fprintf(stderr,"[Landscape Refine QA] Failure frame %s: %s\n",captured?"captured":"unavailable",launch->capture);
        }
        if (destination >= 0&&!authored_cutscene_locked&&!studio.production_freeze()&&!studio.cinematic_preview) {
            fight.leave();look_active=false;SDL_SetRelativeMouseMode(SDL_FALSE);
            current = destination;
            teleport(cam, landmarks[current]);
            current_name=landmarks[current].name;destination_loading=true;
            temporal_cut=true;
            benchmark_x = cam.x;
            benchmark_z = cam.z;
        }

        if (scenic_destination>=0&&!authored_cutscene_locked&&!studio.production_freeze()&&!studio.cinematic_preview) {
            fight.leave();look_active=false;SDL_SetRelativeMouseMode(SDL_FALSE);
            scenic_travel(cam,unsigned(scenic_destination));
            if(editor_quickspot_jump&&std::strcmp(scenic_views[scenic_destination].id,"apartment")){
                // Keep world-design navigation responsive while arriving at
                // the authored close-up, not the generic 120m flight overview.
                cam.fly=true;cam.grounded=false;cam.vy=0;
                cam.y=std::max(world_atlas_height(cam.x,cam.z),0.f)+scenic_views[scenic_destination].eye_height;
                cam.pitch=scenic_views[scenic_destination].pitch;
            }
            current_name=scenic_views[scenic_destination].name;destination_loading=true;
            editor_preview_preparing=editor_quickspot_jump;
            editor_apply_reuse_materials=false;
            temporal_cut=true;
            benchmark_x=cam.x; benchmark_z=cam.z;
        }

        surface_workflow(unsigned(frame));
        live_workflow();
        world_sound_try_apply(render_cam);
        if(studio.apply&&studio.try_fast_object_move(scenery)){
            if(dressing.cover&&!landscape_publish_cover()){launch->exit_code=1;running=false;}
            if(weather_drip_cache&&weather_drips_prepared)world_weather_drips::refresh(*weather_drip_cache,scenery,render_cam.x,render_cam.z,weather_surface,nullptr,true);
            studio_castle_culling=!studio.moved_architecture();temporal_cut=true;
        }
        if(structure_job.valid()&&structure_job.wait_for(std::chrono::seconds(0))==std::future_status::ready){
            auto completed=structure_job.get();studio.structure_cooking=false;structure_last_ms=completed.milliseconds;
            if(completed.ok&&studio.revision==structure_started_revision){
                structure_ready=std::make_unique<StructureCookResult>(std::move(completed));studio.apply=true;
                studio.message="Source collision ready. Publishing matching scenery, floors and blockers.";
            }else if(completed.ok){studio.apply=true;studio.message="Scene changed during collision preparation; preparing the latest draft.";}
            else{++structure_async_failures;++studio.rejected;studio.message=completed.error+" Previous scene and collision preserved.";}
        }
        // Prepare the expensive triangle BVH in a detached CPU world. The
        // temporary scenery snapshot supplies the renderer's exact grounded TRS;
        // its registry and borrowed render buffers are restored before yielding.
        if(studio.apply&&!studio.structure_cooking&&!structure_ready&&(structure_support||!studio.live.structures.empty()||!studio.draft.structures.empty())&&
           (studio.structure_geometry_changed(studio.live,studio.draft)||Studio::terrain_inputs_changed(studio.live,studio.draft))){
            studio.clear_object_preview(scenery);std::string error;bool ok=studio.production_sync(error)&&world_studio::valid(studio.draft,error)&&studio.structure_change_allowed(studio.draft,error)&&studio.components_scene_prevalidate(studio.draft,error);
            const bool terrain_changed=Studio::terrain_inputs_changed(studio.live,studio.draft);bool paused=false;
            auto candidate=studio.draft;WorldScenery *preview=nullptr;world_structure_collision::Capture capture;
            if(ok&&terrain_changed){paused=world_tiles_pause(terrain.stream);ok=paused;if(!ok)error="Could not pause terrain for structural snapshot";}
            if(ok&&terrain_changed){ok=studio.prepare_surface_field(candidate,error,&studio.live);if(ok){world_atlas_set_height_edit(candidate.terrain.empty()&&!candidate.surface_field?nullptr:Studio::height,&candidate);world_atlas_set_water_edit(candidate.waters.empty()&&!candidate.surface_field?nullptr:Studio::water_sample,&candidate);}}
            if(ok){preview=studio.scene(candidate,error,scenery);ok=preview!=nullptr;}
            if(ok)ok=world_structure_collision::capture(studio.project_directory,preview,candidate,studio.structure_mode(),capture,error,structure_ground,nullptr);
            if(preview)world_scenery_free(preview);
            {std::string ignored;studio.configure(studio.live,ignored);}if(terrain_changed)studio.bind();if(paused)world_tiles_resume(terrain.stream);
            if(!ok){studio.apply=false;++studio.rejected;++structure_async_failures;studio.message=error+" Previous scene and collision preserved.";}
            else if(capture.fingerprint!=structure_live_fingerprint){
                studio.apply=false;studio.structure_cooking=true;structure_started_revision=studio.revision;++structure_async_started;
                studio.message="Rebuilding matching source collision in the background. Camera navigation remains available.";
                structure_job=std::async(std::launch::async,cook_structure,std::move(capture));
            }
        }
        if(studio.apply&&!studio.structure_cooking){
            // Revert render-only drag poses before validating committed
            // collision, grounding and shelter changes against the old scene.
            studio.clear_object_preview(scenery);
            studio.apply=false;std::string error;
            if(!studio.production_retry_components()){structure_ready.reset();studio.production_restore_failed=true;studio.rejected++;}
            else if(!studio.production_sync(error)||!world_studio::valid(studio.draft,error)||!studio.structure_change_allowed(studio.draft,error)||!studio.components_scene_prevalidate(studio.draft,error)){structure_ready.reset();studio.production_restore_failed=studio.production_restore;studio.rejected++;studio.message=error;}
            else{
                const bool rebuild=studio.rebuild||studio.object_lighting_pending||!world_surface::same_paths(studio.draft.surfaces,studio.live.surfaces);
                // Shutdown joins the worker before callbacks/documents change.
                const bool terrain_rebuild=studio.terrain_inputs_changed(studio.draft,studio.live);
                bool ecology_changed=studio.draft.ecology.size()!=studio.live.ecology.size();
                if(!ecology_changed)for(size_t i=0;i<studio.live.ecology.size();++i)if(!world_ecology::same(studio.live.ecology[i],studio.draft.ecology[i])){ecology_changed=true;break;}
                const auto apply_started=std::chrono::steady_clock::now();
                if(terrain_rebuild&&!world_tiles_pause(terrain.stream)){launch->exit_code=1;running=false;studio.message="Could not pause terrain streaming.";break;}
                auto next=studio.draft;const bool field_ready=!terrain_rebuild||studio.prepare_surface_field(next,error,&studio.live);if(!terrain_rebuild)next.surface_field=studio.live.surface_field;
                if(terrain_rebuild)world_atlas_set_height_edit(next.terrain.empty()&&!next.surface_field?nullptr:Studio::height,&next);
                if(terrain_rebuild)world_atlas_set_water_edit(next.waters.empty()&&!next.surface_field?nullptr:Studio::water_sample,&next);
                const auto height_changes=terrain_rebuild&&field_ready?world_studio_height_changes::changed(studio.live,next):std::vector<world_studio_height_changes::Rect>{};
                const auto field_finished=std::chrono::steady_clock::now();
                WorldScenery *next_scene=rebuild&&field_ready?studio.scene(next,error,scenery):nullptr;
                LandscapeMaterials next_landscape;WorldSurfaceGPU next_surfaces;bool prepared=field_ready&&(!rebuild||next_scene);
                const auto scene_finished=std::chrono::steady_clock::now();
                bool forest_ground_changed=false;
                const auto &fb=landscape.forest_shelter.bounds;
                for(const auto &r:height_changes)if(r.xmax>=fb[0]&&r.xmin<=fb[0]+1./fb[2]&&r.zmax>=fb[1]&&r.zmin<=fb[1]+1./fb[3]){forest_ground_changed=true;break;}
                const bool masks_changed=studio.object_lighting_pending||forest_ground_changed||(rebuild&&next_scene&&Studio::shelter_changed(scenery,next_scene));
                const bool surfaces_changed=masks_changed||Studio::surface_changed(next.surfaces,studio.live.surfaces);
                if(prepared&&masks_changed)prepared=next_landscape.init(rebuild?next_scene:scenery,showcase_terrain.side>0);
                if(prepared&&surfaces_changed)prepared=next_surfaces.prepare(masks_changed?next_landscape.masks:landscape.masks,next.surfaces,error);
                const auto masks_finished=std::chrono::steady_clock::now();
                if(prepared&&(structure_support||structure_ready||!studio.live.structures.empty()||!next.structures.empty())&&(structure_ready||studio.structure_geometry_changed(studio.live,next)||terrain_rebuild)){
                    world_structure_collision::Capture check;
                    prepared=world_structure_collision::capture(studio.project_directory,rebuild?next_scene:scenery,next,studio.structure_mode(),check,error,structure_ground,nullptr);
                    const auto expected=structure_ready?structure_ready->fingerprint:structure_live_fingerprint;
                    if(prepared&&check.fingerprint!=expected){
                        std::fprintf(stderr,"[Structure Collision] Capture mismatch staged=%016llx prepared=%016llx rebuild=%d revision=%llu/%llu\n",(unsigned long long)expected,(unsigned long long)check.fingerprint,int(rebuild),(unsigned long long)structure_started_revision,(unsigned long long)studio.revision);
                        if(structure_ready){const auto &before=structure_ready->source.instances;const auto &after=check.instances;unsigned differences=0;
                            std::fprintf(stderr,"[Structure Collision] Capture instances staged=%zu prepared=%zu\n",before.size(),after.size());
                            for(std::size_t i=0;i<std::min(before.size(),after.size())&&differences<8;++i){const auto &a=before[i],&b=after[i];
                                const double av[]={a.pose.x,a.pose.y,a.pose.z,a.pose.yaw,a.pose.scale,a.pose.pitch,a.pose.roll,a.pose.sx,a.pose.sy,a.pose.sz};
                                const double bv[]={b.pose.x,b.pose.y,b.pose.z,b.pose.yaw,b.pose.scale,b.pose.pitch,b.pose.roll,b.pose.sx,b.pose.sy,b.pose.sz};
                                if(a.alias==b.alias&&a.model==b.model&&!std::memcmp(av,bv,sizeof av))continue;
                                ++differences;std::fprintf(stderr,"[Structure Collision] Instance%zu staged=%s model=%s prepared=%s model=%s\n",i,a.alias.c_str(),a.model.c_str(),b.alias.c_str(),b.model.c_str());
                                static const char *names[]={"x","y","z","yaw","scale","pitch","roll","sx","sy","sz"};
                                for(unsigned k=0;k<10;++k)if(std::memcmp(&av[k],&bv[k],sizeof(double)))std::fprintf(stderr,"[Structure Collision]   %s staged=%.17g (%a) prepared=%.17g (%a)\n",names[k],av[k],av[k],bv[k],bv[k]);
                            }
                            if(!differences&&before.size()==after.size())std::fprintf(stderr,"[Structure Collision] Instance bytes match; source/catalog file revision metadata changed.\n");
                        }
                        std::fflush(stderr);prepared=false;error="Prepared source geometry changed while collision was cooking. Apply again to prepare the current draft.";
                    }
                }
                WorldStructureGPU next_structure_gpu;
                if(prepared&&structure_ready)prepared=next_structure_gpu.prepare(structure_ready->source.procedural,error);
                Studio::RuntimePrepared runtime_next;
                if(prepared){const auto *previous=active_playtest_walk;const bool previous_policy=active_structure_world;
                    if(structure_ready){active_playtest_walk=structure_ready->support?structure_ready->support.get():structure_baseline_walk;active_structure_world=structure_ready->support?true:structure_baseline_policy;}
                    prepared=studio.prepare_runtime(next,rebuild?next_scene:scenery,runtime_next,error);active_playtest_walk=previous;active_structure_world=previous_policy;}
                const bool features=!next.materials.empty()||!next.lights.empty()||!next.decals.empty()||studio.preview_lod>=0;
                WorldEnvironment *next_environment=nullptr;
                const bool replace_environment=next.environment.enabled&&(!studio_environment||next.environment.path!=studio.live.environment.path);
                if(prepared&&replace_environment){next_environment=world_environment_create(next.environment.path.c_str(),error);prepared=next_environment!=nullptr;}
                const bool shader_rebuild=studio.reload_shaders||(!next.surfaces.winds.empty())!=studio_wind_features||WorldSurfaceGPU::features(next.surfaces)!=studio_imported_features||features!=studio_shader_features||next.environment.enabled!=studio_environment_features||Studio::graphs_changed(next,studio.live)||next.waters.empty()!=studio.live.waters.empty();
                if(prepared&&shader_rebuild){
                    prepared=reload_studio_shaders(features,next.environment.enabled,&next,true);if(!prepared)error="Shader link failed. Previous programs and scene preserved.";
                }
                if(prepared&&rebuild&&!world_scenery_prepared_valid(next_scene,scenery)){prepared=false;error="Prepared scene owner changed; previous scene preserved.";}
                if(prepared&&!studio.commit_runtime(runtime_next,scene_particles,torch_flames)){
                    prepared=false;error="Runtime publication failed; previous scene preserved.";
                    // No scenery ownership has moved yet. Restore the known
                    // live runtime too if a later runtime target rejected.
                    if(terrain_rebuild)studio.bind();
                    Studio::RuntimePrepared previous;std::string restore_error;
                    if(!studio.prepare_runtime(studio.live,scenery,previous,restore_error)||!studio.commit_runtime(previous,scene_particles,torch_flames)){launch->exit_code=1;running=false;error+=" Runtime restoration failed: "+restore_error;}
                }
                // Only render-resource ownership remains after validation and
                // runtime commit. That commit cannot modify either scenery,
                // so the read-only ownership check above remains true here.
                if(prepared&&rebuild&&!world_scenery_commit_prepared(next_scene,scenery)){prepared=false;launch->exit_code=1;running=false;error="Scenery ownership invariant changed during atomic publication";}
                finish_programs(prepared);
                studio.reload_shaders=false;studio.fail_shader_test=false;
                if(prepared){
                    if(structure_ready){structure_gpu.release();structure_gpu=std::move(next_structure_gpu);next_structure_gpu={};structure_live_fingerprint=structure_ready->fingerprint;structure_support=std::move(structure_ready->support);active_playtest_walk=structure_support?structure_support.get():structure_baseline_walk;active_structure_world=structure_support?true:structure_baseline_policy;active_city_physics=structure_support?nullptr:structure_baseline_city;++structure_async_commits;}
                    if(surfaces_changed){studio_surface_gpu.release();studio_surface_gpu=std::move(next_surfaces);}
                    if(replace_environment){world_environment_free(studio_environment);studio_environment=next_environment;}
                    if(!next.environment.enabled){world_environment_free(studio_environment);studio_environment=nullptr;}
                    if(terrain_rebuild){studio.live=std::move(next);studio.bind();}
                    else{
                        // Tile workers read only live.terrain. Scalar look and
                        // render-thread-only data can commit without changing
                        // that vector or rebuilding unchanged model buffers.
                        studio.live.placements=std::move(next.placements);studio.live.entities=std::move(next.entities);studio.live.source_signature=next.source_signature;studio.live.look=next.look;studio.live.lights=std::move(next.lights);studio.live.materials=std::move(next.materials);studio.live.decals=std::move(next.decals);studio.live.splines=std::move(next.splines);studio.live.groups=std::move(next.groups);studio.live.assets=std::move(next.assets);studio.live.environment=std::move(next.environment);studio.live.overrides=std::move(next.overrides);studio.live.actors=std::move(next.actors);studio.live.navigation=std::move(next.navigation);studio.live.particles=std::move(next.particles);studio.live.sounds=std::move(next.sounds);studio.live.weather_global=next.weather_global;studio.live.weather_areas=std::move(next.weather_areas);studio.live.graphs=std::move(next.graphs);studio.live.surfaces=std::move(next.surfaces);studio.live.ecology=std::move(next.ecology);studio.live.structures=std::move(next.structures);studio.live.next_structure_id=next.next_structure_id;
                    }
                    if(rebuild){world_scenery_free(scenery);scenery=next_scene;studio.refresh_scene(scenery);}if(masks_changed){landscape.release();landscape=std::move(next_landscape);}
                    if(terrain_rebuild||ecology_changed){horizon.target_x=horizon.target_z=1e30;horizon.building=false;horizon.has_selection=false;}
                    if(terrain_rebuild){
                        // Match distant terrain shadow/weather lookup to the
                        // committed collision/mesh heights, including undo.
                        WorldVillageUnpackScope unpack;glBindTexture(GL_TEXTURE_2D,macro_height);
                        for(const auto &r:height_changes){
                            double u0,v0,u1,v1;world_atlas_world_to_uv(r.xmin,r.zmin,&u0,&v0);world_atlas_world_to_uv(r.xmax,r.zmax,&u1,&v1);
                            const int xa=std::clamp(int(std::ceil(u0*1024)),0,1024),xb=std::clamp(int(std::floor(u1*1024)),0,1024);
                            const int za=std::clamp(int(std::ceil(v0*576)),0,576),zb=std::clamp(int(std::floor(v1*576)),0,576);
                            if(xa>xb||za>zb)continue;
                            const int w=xb-xa+1,h=zb-za+1;std::vector<float> heights(size_t(w)*h);
                            for(int iz=za;iz<=zb;++iz)for(int ix=xa;ix<=xb;++ix)heights[size_t(iz-za)*w+ix-xa]=world_atlas_sample_uv(double(ix)/1024,double(iz)/576).height;
                            glTexSubImage2D(GL_TEXTURE_2D,0,xa,za,w,h,GL_RED,GL_FLOAT,heights.data());
                        }
                    }
                    studio_castle_culling=!studio.moved_architecture();if(rebuild)refresh_tavern_shell();if(terrain_rebuild||masks_changed||rebuild)world_weather_invalidate_surface(weather);
                    if(weather_drips_prepared&&weather_drip_cache){if(rebuild)prepare_weather_drips(render_cam);else if(terrain_rebuild)world_weather_drips::refresh(*weather_drip_cache,scenery,render_cam.x,render_cam.z,weather_surface,nullptr,true);}
                    std::string weather_error;std::vector<world_weather_authoring::BoundArea> next_weather;if(world_weather_prepare(studio.live,next_weather,weather_error))world_weather_commit(studio.live,std::move(next_weather),&render_cam);else{++authored_weather_failures;std::fprintf(stderr,"[Weather] Apply: %s\n",weather_error.c_str());}
                    std::string sound_error;if((world_sound_active||!studio.cinematic_audio_active)&&(!studio.live.sounds.empty()||world_sound_active)&&!world_sound_prepare(studio.live,sound_error)){studio.message="World applied; map audio unchanged: "+sound_error;std::fprintf(stderr,"[World audio] Apply: %s\n",sound_error.c_str());}
                    studio.materials(scenery);if(terrain_rebuild)world_cover_set_filter(dressing.cover,studio.live.stamps.empty()&&!studio.live.surface_field?nullptr:Studio::cover,&studio.live);
                    if(terrain_rebuild||ecology_changed||masks_changed){if(!landscape_publish_cover()){launch->exit_code=1;running=false;studio.message="Could not publish landscape growth.";}}
                    const auto &l=studio.live.look;hour=l.hour;cloud=l.cloud;rain=l.rain;wind=l.wind;mist=l.fog;bearing=l.bearing;sun_strength=l.sun;sky_fill=l.sky;exposure=l.exposure;
                    for(auto *&cache:local_probe_regions){world_local_probes_free(cache);cache=nullptr;}
                    studio.object_lighting_pending=false;studio.applied++;studio.dirty=studio.rebuild=false;studio.production_applied();const bool component_commit=studio.components_scene_applied();if(rebuild)live_visual_generation=0;if(component_commit)studio.message="Applied. Rebuilding streamed roots and local lighting; Save stores this world.";else{active_world_runtime=nullptr;studio.components_ready=false;studio.open=true;}
                    if(!sound_error.empty())studio.message="World applied; map audio unchanged: "+sound_error;
                    if(studio.target)studio.y=world_atlas_height(studio.x,studio.z);
                    if(!studio.open){cam.y=std::max(cam.y,double(world_atlas_walk_height(cam.x,cam.z))+1.75);}temporal_cut=true;destination_loading=true;
                    editor_preview_preparing=studio.open&&terrain_rebuild;
                    editor_apply_reuse_materials=studio.open&&terrain_rebuild&&!shader_rebuild&&main_material_warm.ready;
                }else{
                    if(replace_environment&&next_environment)world_environment_free(next_environment);
                    if(next_scene)world_scenery_free(next_scene);
                    next_surfaces.release();next_landscape.release();if(terrain_rebuild)studio.bind();if(rebuild){std::string ignored;studio.configure(studio.live,ignored);}
                    studio.production_restore_failed=studio.production_restore;studio.rejected++;studio.message=error.empty()?"Could not prepare lighting masks; previous scene preserved.":error;
                }
                if(structure_ready){if(!prepared)++structure_async_failures;structure_ready.reset();}
                next_structure_gpu.release();
                studio.last_apply_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-apply_started).count();studio.last_apply_terrain=terrain_rebuild;studio.last_apply_masks=masks_changed;if(terrain_rebuild)studio.terrain_rebuild_count++;else studio.terrain_rebuild_skipped++;
                size_t changed_tiles=0;
                if(terrain_rebuild){
                    WorldTileRequest before[WORLD_TILES_CAPACITY],after[WORLD_TILES_CAPACITY];
                    const auto count=world_tiles_desired(terrain.stream,before,WORLD_TILES_CAPACITY);
                    if(prepared)for(const auto &r:height_changes)world_tiles_invalidate_bounds(terrain.stream,r.xmin,r.zmin,r.xmax,r.zmax);
                    world_tiles_desired(terrain.stream,after,WORLD_TILES_CAPACITY);
                    for(size_t i=0;i<count;++i)if(before[i].generation!=after[i].generation)++changed_tiles;
                    world_tiles_resume(terrain.stream);
                }
                const auto done=std::chrono::steady_clock::now();
                auto elapsed_ms=[](auto a,auto b){return std::chrono::duration<double,std::milli>(b-a).count();};
                std::fprintf(stderr,"[Terrain Apply] commit=%u ok=%d total=%.3fms field=%.3f scene=%.3f masks=%.3f rest=%.3f tiles=%zu/%zu masks_rebuilt=%d height_regions=%zu\n",studio.applied,int(prepared),elapsed_ms(apply_started,done),elapsed_ms(apply_started,field_finished),elapsed_ms(field_finished,scene_finished),elapsed_ms(scene_finished,masks_finished),elapsed_ms(masks_finished,done),changed_tiles,terrain.desired.size(),int(masks_changed),height_changes.size());
                if(prepared){if(editor_apply_perf.size()==editor_apply_perf_limit)editor_apply_perf.pop_front();++editor_apply_perf_total;editor_apply_perf.push_back({double(studio.applied),elapsed_ms(apply_started,done),double(changed_tiles),double(masks_changed),0});editor_apply_wait_started=done;}
                std::fflush(stderr);
                quality_valid_after=frame+120;
            }
        }
        studio.vfx_update(scene_particles,torch_flames);
        g_engine_perf.raw_editor_ms = millis(ui_begin);
        g_engine_perf.editor_ms = g_engine_perf.raw_editor_ms;
        g_engine_perf.static_assets = 0;
        g_engine_perf.static_instances = 0;
        g_engine_perf.static_gpu_bytes = 0;
        engine_perf_frame();
        const Uint64 frame_capture_hooks_begin=SDL_GetPerformanceCounter();
        grandstone_obstacle_capture();
        opening_prologue_qa_capture();
        npc_intro_qa_capture(display_w,display_h,frame);
        road_traffic_qa_capture(display_w,display_h,frame);
        first_journey_qa_capture();
        sword_stick_qa_capture();
        arena_qa_capture();
        orc_roster_qa_capture(display_w,display_h);
        feedback_qa_capture(display_w,display_h);
        blade_clash_qa_capture(display_w,display_h);
        joust_qa_capture(display_w,display_h);
        ultra_ui_capture(display_w,display_h);
        equipment_qa_capture(display_w,display_h);
        animation_polish_qa_capture(display_w,display_h,frame);
        duel_qa_capture(display_w,display_h,frame,render_cam);
        roster_qa_capture(display_w,display_h);
        severing_capture();
        engine_launch_frame(window);
        const double frame_capture_hooks_ms=millis(frame_capture_hooks_begin);
        Uint64 swap = SDL_GetPerformanceCounter();
        // Native paired tests improve pacing with completion at the VSync
        // boundary in the village and exterior dragon observation clearings.
        // Keep the GPU-heavy jungle's original CPU/GPU overlap. The roost
        // policy depends only on camera position, even with dragons disabled;
        // entry/exit hysteresis avoids switching on every boundary crossing.
        // All completion waits remain included in measured frame intervals.
        roost_presentation=world_castle_dragons_roost_presentation_local(
            cam.x-presentation_origin_x,cam.z-presentation_origin_z,roost_presentation)!=0;
        if(complete_before_swap&&(world_atlas_village_weight(cam.x,cam.z)>0.f||roost_presentation)){
            glFinish();completion_wait_ms+=millis(swap);completed_presentation_frames++;
            if(roost_presentation)roost_completion_frames++;
        }
        SDL_GL_SwapWindow(window);
        g_engine_perf.present_ms = millis(swap);
        // Pace after presentation so the adaptive sample includes this wait
        // while active CPU/GPU stage timings continue to exclude it.
        const bool qa_frame_limit=launch->frames&&std::getenv("VALWOOD_QA_FRAME_LIMIT")&&std::strcmp(std::getenv("VALWOOD_QA_FRAME_LIMIT"),"1")==0;
        const int pacing_refresh=graphics_monitor_refresh;
        const bool vsync_limits_rate=actual_swap_interval>0&&pacing_refresh>0&&pacing_refresh<=graphics.frame_rate_limit;
        if(!opening_qa&&(!launch->frames||qa_frame_limit)&&graphics.frame_rate_limit>0&&!vsync_limits_rate){
            const Uint64 frequency=SDL_GetPerformanceFrequency();
            const Uint64 interval=(frequency+Uint64(graphics.frame_rate_limit)-1)/Uint64(graphics.frame_rate_limit);
            Uint64 now=SDL_GetPerformanceCounter();
            while(now-frame_start<interval){
                const double remaining_ms=1000.*double(interval-(now-frame_start))/double(frequency);
                SDL_Delay(Uint32(std::max(1.,std::floor(remaining_ms))));
                now=SDL_GetPerformanceCounter();
            }
        }
        adaptive_queries.cadence(frame,millis(frame_start)*.001);
        if(destination_wait_mask){const double wait_ms=millis(frame_start);for(unsigned i=0;i<10;++i)if(destination_wait_mask&(1u<<i))destination_wait_ms[i]+=wait_ms;}
        if(source_review.active){
            source_review.record(frame,cam,source_review_waiting_frame||!view_ready||destination_loading,millis(frame_start),g_engine_perf.render_total_gpu_ms);
            if(source_review.complete)running=false;
        }
        if(frame_phase_profile.file){
            const bool phase_cinematic=cutscene_frame.active||(opening_enabled&&opening_frame.cinematic);
            const bool phase_pause=presentation&&preview_screen!=1;
            const bool phase_loading=!view_ready||destination_loading||ui_loading;
            const bool phase_modal=phase_cinematic||phase_pause||studio.open||show_map||show_graphics||inventory_open||first_journey_dialogue_active||(opening_enabled&&opening_frame.camp_dialogue);
            const bool phase_controls=player_controls_allowed&&(!opening_enabled||opening_controls_enabled)&&!phase_modal&&!phase_loading;
            frame_phase_profile.record(frame,millis(frame_start),frame_capture_hooks_ms,phase_controls,phase_loading,phase_modal,phase_cinematic,studio.open,inventory_open,phase_pause,cam.x,cam.z,g_engine_perf.present_ms);
        }
    }

    animation_polish_qa_finish();
    if(qa_quickspot_stream&&qa_quickspot_step!=3){std::fprintf(stderr,"[Quick Spots QA] FAIL: only %u/3 destinations became editable\n",qa_quickspot_step);launch->exit_code=1;}
    if(qa_terrain_apply&&qa_terrain_apply_phase!=2){std::fprintf(stderr,"[Terrain Apply QA] FAIL: Apply did not commit\n");launch->exit_code=1;}
    if(!source_review.finish()){std::fprintf(stderr,"[Source review] Failed to write report\n");launch->exit_code=1;}
    if(launch->editor&&launch->frames&&std::getenv("VALWOOD_QA_ASSET_BROWSER")&&!browser_qa.passed){browser_qa.report();launch->exit_code=1;}
    engine_perf_finish();
    // Opt-in inventory runs after the last measured frame, before resources
    // are destroyed. Ordinary play performs no additional GL queries.
    const std::string gpu_budget_path=std::string(launch->profile[0]?launch->profile:
        launch->capture[0]?launch->capture:"valwood")+".gpu-budget.json";
    const auto trail_camp_state=world_trail_camp_stats(trail_camp);
    const auto arrival_camp_state=world_trail_camp_stats(arrival_camp);
    const auto first_journey_town_state=world_trail_camp_stats(first_journey_townsfolk);
    const auto gpu_budget_audit=world_gpu_budget_audit(gpu_budget_path.c_str());
    if(gpu_budget_audit.requested&&(!gpu_budget_audit.available||!gpu_budget_audit.saved||!gpu_budget_audit.complete))
        launch->exit_code=1;
    {const auto &a=aquatic_system.stats;const auto &r=aquatic_renderer.stats;const auto m=world_combat_motion_stats(combat_motion);
     std::printf("[Aquatics] fish%u spawned%llu flee%llu entries%llu strokes%llu bubbles%llu fishDraws%llu effectsDraws%llu animationLoaded%d active%d mode%u uploads%llu\n",a.fish,(unsigned long long)a.fish_spawned,(unsigned long long)a.fish_fled,(unsigned long long)a.entries,(unsigned long long)a.strokes,(unsigned long long)a.bubbles_spawned,(unsigned long long)r.fish_draws,(unsigned long long)r.surface_draws,m.aquatic_loaded?1:0,m.aquatic_active?1:0,m.aquatic_mode,(unsigned long long)m.aquatic_uploads);}
    std::printf("[World] End position %.3f %.3f %.3f yaw %.3f fly %d\n",
        cam.x, cam.y, cam.z, cam.yaw, cam.fly ? 1 : 0);
    WorldTilesMetrics metrics = {};

    world_tiles_metrics(terrain.stream, &metrics);
    std::printf("[World] Stream peakCPU %.2f MiB GPU %.2f MiB desired %zu generated %llu cancelled %llu failed %llu\n",
        metrics.peak_cpu_bytes / 1048576., terrain.bytes() / 1048576., metrics.desired, (unsigned long long) metrics.generated_total,
        (unsigned long long) metrics.cancelled_total, (unsigned long long) metrics.failed_total);
    size_t terrain_gpu_bytes = terrain.bytes();
    const size_t procedural_shadow_proxies=dressing.shadow_proxy_count();
    const bool shadow_mesh_lod=dressing.shadow_lod,shadow_proxies_synced=dressing.shadow_proxies_synced();
    int reflection_width=reflection.w,reflection_height=reflection.h;
    const int shadow_side=shadow.side;
    const int graphics_render_w=target.w,graphics_render_h=target.h;
    const bool atlas_map_loaded=materials[7]!=0;
    auto authored_state=world_scenery_stats(scenery);
    const auto live_stream_state=world_scenery_streaming_stats(scenery);
    if(source_review.active&&live_stream_state.failed)
        std::fprintf(stderr,"[Source review] %zu streaming failures: %s\n",live_stream_state.failed,live_stream_state.error);
    const auto live_system_state=studio.components_runtime?studio.components_runtime->stats():world_runtime::Stats{};
    const auto live_animation_state=live_visuals&&live_visuals->animation?live_visuals->animation->stats():world_runtime_animation::Stats{};
    const auto live_visual_draws=live_visuals?live_visuals->draws:0,live_visual_moves=live_visuals?live_visuals->moved:0;
    const unsigned live_rigid_count=live_visuals?live_visuals->rigid_count:0;
    struct SourceReport {std::string path;WorldScenerySourceStats stats;};
    std::vector<SourceReport> source_reports;
    for(size_t i=0;i<authored_state.source_files;i++) {
        auto source=world_scenery_source_stats(scenery,i);
        source_reports.push_back({source.path?source.path:"",source});
    }
    const auto grass_frustum_culled=dressing.grass.culled_frustum,grass_wall_culled=dressing.grass.culled_wall;
    const auto scanned_frustum_culled=dressing.scanned_culled_frustum,scanned_wall_culled=dressing.scanned_culled_wall;
    const auto population_state=world_village_population_stats(population);
    const auto road_traffic_state=world_road_traffic_stats(road_traffic);
    const auto training_state=world_castle_training_stats(training);
    const auto watch_state=world_village_watch_stats(watch);
    npc_intro_qa_finish();
    road_traffic_qa_finish();
    walking_controls_finish();
    minimap_report();
    grass_interaction_report();
    if(qa_studio&&qa_studio_phase!=5){std::fprintf(stderr,"[Studio QA] incomplete phase %u\n",qa_studio_phase);launch->exit_code=1;}
    const auto pets_state=world_village_pets_stats(pets);
    const auto wildlife_state=world_wildlife_stats(wildlife);
    const auto wildlife_preparation=world_wildlife_preparation_stats(wildlife);
    const auto bandit_state=world_bandit_camp_stats(bandits);
    const auto stronghold_state=world_stronghold_stats(stronghold);
    const auto snow_state=world_snow_stats(snow);
    const auto wildlife_scene_cache=texture_cache_stats();
    const auto texture_block_state=world_model_texture_block_stats();
    const auto quality_state=quality.status();
    const auto torch_state=world_torch_flames_stats(torch_flames);
    const auto weather_state=world_weather_stats(weather);
    const auto authored_storm_stats=authored_storm.stats;const auto lightning_draws=lightning_visual.draws;lightning_visual.free();
    const auto particle_state=world_scene_particles_stats(scene_particles);
    const bool training_audio_ready=training_sounds.ready;const unsigned training_audio_events=training_sounds.events;
    const unsigned training_body_audio=training_sounds.body_events,training_metal_audio=training_sounds.metal_events;
    studio.living_shutdown(!launch->frames&&!launch->benchmark);
    const auto demo_audio_state=world_demo_audio_stats(demo_audio);
    const auto world_sound_state=world_cinematics_audio::stats(studio.cinematic_audio);
    const auto sky_state=world_sky_stats(sky);const auto occlusion_state=world_occlusion_stats(occlusion);
    const auto bloom_state=world_bloom_stats(bloom);
    const uint64_t indirect_target_bytes=uint64_t(target.w)*target.h*8;
    const auto village_directional_bytes=landscape.detail.directional_bytes;
    const auto village_history_bytes=landscape.detail.history_bytes;
    const int village_directional_side=landscape.detail.directional_side,village_directional_slices=landscape.detail.directional_slices;
    const bool village_directional_active=landscape.directional_enabled&&landscape.detail.directional&&landscape.detail.up;
    const bool village_history_active=landscape.history_enabled&&landscape.detail.history;
    const bool castle_practicals_active=landscape.castle_lights_ready;
    bool temporal_enabled=temporal!=nullptr;
    WorldLocalProbeStats local_probe_state={};unsigned local_probe_cache_regions=0;
    for(auto *probes:local_probe_regions)if(probes) {
        const auto s=world_local_probes_stats(probes);local_probe_cache_regions++;
        local_probe_state.count+=s.count;local_probe_state.ready+=s.ready;local_probe_state.bytes+=s.bytes;
        local_probe_state.capture_calls+=s.capture_calls;local_probe_state.capture_faces+=s.capture_faces;
        local_probe_state.readbacks+=s.readbacks;local_probe_state.bind_calls+=s.bind_calls;local_probe_state.failures+=s.failures;
        world_local_probes_free(probes);
    }
    dragon_queries.release();adaptive_queries.release();
    std::printf("[World] Camera mode %s, toggles %u\n",third_person?"third":"first",camera_toggles);
    world_village_population_free(population);
    active_road_traffic=nullptr;world_road_traffic_free(road_traffic);road_traffic=nullptr;
    const auto combat_motion_state=world_combat_motion_stats(combat_motion);
    const auto arena_orc_bank_state=world_combat_motion_opponents_stats(arena_orc_motion);
    const auto arena_orc_motion_state=world_combat_motion_stats(arena_orc_motion);
    std::fprintf(stderr,"[Arena Orc roster] available=%u entries=%u selected=%s loaded=%s resident=%u peak=%u loads=%u\n",
        unsigned(arena_orc_catalog_ready),unsigned(arena_orc_roster.entries.size()),
        arena_orc_catalog_ready?arena_orc_roster.entries[unsigned(arena_orc_character_draft)].id.c_str():"gnur",
        arena_orc_character_loaded.c_str(),arena_orc_resident_banks,arena_orc_peak_banks,arena_orc_bank_loads);
    const auto combat_reaction_physics_state=world_combat_motion_anatomy_stats(arena_opponent_motion());
    const auto combat_reaction_death_state=world_combat_motion_death_stats(arena_opponent_motion());
    const auto grip_stats=world_combat_motion_grip_stats(combat_motion);
    const auto continuity_stats=world_combat_motion_continuity_stats(combat_motion);
    std::fprintf(stderr,"[Arena opponents] committed=%s Orcs_ready=%u body_poses=%u arm_poses=%u measured_pivots=%u certified_weapon_pairs=%u actors=%u uploads=%llu original_textures=%u walk_stride_m=%.3f body_radius_m=%.3f body_height_m=%.3f\n",
        world_arena_opponents::name(arena_game.config().opponents),unsigned(arena_orc_assets_ready),arena_orc_bank_state.body_poses,
        arena_orc_bank_state.arm_poses,arena_orc_bank_state.measured_pivots,arena_orc_bank_state.certified_weapon_pairs,
        arena_orc_motion_state.actors,arena_orc_motion_state.uploads,unsigned(arena_orc_original_textures_ready),arena_orc_bank_state.walk_stride_metres,arena_game.opponent_body_radius(),arena_game.opponent_body_height());
    {const auto s=world_combat_motion_variant_stats(combat_motion);std::fprintf(stderr,"[Arena knight variants] loaded=%u mask=%u body=%llu,%llu,%llu,%llu crowd=%llu,%llu,%llu,%llu pieces=%llu,%llu,%llu,%llu support=%llu,%llu,%llu,%llu\n",unsigned(s.loaded),s.available_mask,
        (unsigned long long)s.body_draws[0],(unsigned long long)s.body_draws[1],(unsigned long long)s.body_draws[2],(unsigned long long)s.body_draws[3],
        (unsigned long long)s.crowd_draws[0],(unsigned long long)s.crowd_draws[1],(unsigned long long)s.crowd_draws[2],(unsigned long long)s.crowd_draws[3],
        (unsigned long long)s.piece_draws[0],(unsigned long long)s.piece_draws[1],(unsigned long long)s.piece_draws[2],(unsigned long long)s.piece_draws[3],
        (unsigned long long)s.support_captures[0],(unsigned long long)s.support_captures[1],(unsigned long long)s.support_captures[2],(unsigned long long)s.support_captures[3]);}
    const auto combat_vfx_state=combat_vfx.stats();const auto combat_vfx_render_state=world_combat_vfx_stats(combat_vfx_renderer);
    const auto blood_state=blood_effects.stats();const auto blood_render_state=world_blood_stats(blood_renderer);
    const auto sever_blood_state=sever_blood.stats();
    const auto arena_render_state=world_arena_render_stats(arena_renderer);
    aquatic_renderer.free();
    const auto skeletal_hero_state=skeletal_hero.stats();
    const bool skeletal_story_swim_available=skeletal_hero.supports_swimming(),skeletal_story_death_available=skeletal_hero.supports_death();
    {const auto &state=skeletal_hero_state;std::fprintf(stderr,"[Skeletal hero candidate] updates=%llu uploads=%llu draws=%llu attacks=%llu failures=%llu cycles=%.3f speed=%.3f run=%.3f contacts=%llu contact_samples=%llu contact_failures=%llu foot_plants=%llu foot_releases=%llu foot_reach_releases=%llu maximum_foot_residual_m=%.6f maximum_foot_correction_m=%.6f turn_steps=%llu turn_weight=%.6f turn_debt=%.6f\n",(unsigned long long)state.updates,(unsigned long long)state.uploads,(unsigned long long)state.draws,(unsigned long long)state.attacks,(unsigned long long)state.failures,state.gait_cycles,state.speed,state.run,skeletal_contact_queries,skeletal_contact_samples,skeletal_contact_failures,(unsigned long long)state.foot_plants,(unsigned long long)state.foot_releases,(unsigned long long)state.foot_reach_releases,state.maximum_foot_residual_m,state.maximum_foot_correction_m,(unsigned long long)state.turn_steps,state.turn_weight,state.turn_debt_radians);}
    std::fprintf(stderr,"[Skeletal first person] draws=%llu failures=%llu\n",(unsigned long long)skeletal_first_person_draws,(unsigned long long)skeletal_first_person_failures);
    skeletal_hero.release();skeletal_first_person.free();
    arena_actor_banks.fill(nullptr);arena_player_bank=nullptr;arena_appearance_banks.clear();
    joust_qa_finish();
    ultra_ui_finish();
    joust_splinters.release();joust_renderer.release();
    world_arena_render_free(arena_renderer);world_combat_vfx_free(combat_vfx_renderer);world_blood_free(blood_renderer);world_combat_motion_free(combat_motion);world_combat_motion_free(arena_orc_motion);world_combat_motion_free(battlefield_combat_motion);
    world_castle_training_free(training);training_weapon.release();
    world_village_watch_free(watch);
    world_village_pets_free(pets);
    world_wildlife_free(wildlife);
    world_trail_camp_free(trail_camp);world_trail_camp_free(arrival_camp);world_trail_camp_free(first_journey_townsfolk);
    std::array<WorldCastleDragon,2> dragon_final={};
    for(unsigned i=0;i<2;i++)world_castle_dragons_actor(dragons,i,&dragon_final[i]);
    const auto dragon_state=world_castle_dragons_stats(dragons);
    if(dragon_breath)dragon_breath_accumulated=world_dragon_breath_stats(dragon_breath);
    world_castle_dragons_free(dragons);world_dragon_breath_free(dragon_breath);
    world_bandit_camp_free(bandits);
    active_stronghold=nullptr;world_stronghold_free(stronghold);world_snow_free(snow);
    battlefield_audio.release();
    battlefield_release_render();
    world_torch_flames_free(torch_flames);
    const bool generated_grass_shader_linked=foliage.generated_grass!=nullptr;
    active_live_visuals=nullptr;active_world_runtime=nullptr;if(live_visuals)live_visuals->release();live_programs.free();previous_live.free();
    // Consume every detached query world on its receiving owner thread,
    // including a close request while a background cook is still running.
    if(structure_job.valid()){auto retired=structure_job.get();studio.structure_cooking=false;}
    structure_ready.reset();
    const auto structure_gpu_bytes=structure_gpu.bytes,structure_gpu_triangles=structure_gpu.triangles;
    const unsigned structure_gpu_walls=structure_gpu.walls,structure_gpu_bridges=structure_gpu.bridges,structure_gpu_segments=structure_gpu.segments,structure_gpu_draws=structure_gpu.draws,structure_gpu_shadow_draws=structure_gpu.shadow_draws;
    structure_gpu.release();
    std::printf("[Structure Collision] async_started=%u commits=%u failures=%u last_ms=%.3f\n",structure_async_started,structure_async_commits,structure_async_failures,structure_last_ms);
    const auto asset_browser_stats=studio.designer_asset_thumbnails.stats();
    std::printf("[Asset Browser] textured_previews=%llu cached_tiles=%u preview_bytes=%zu pending=%u unavailable=%u\n",(unsigned long long)asset_browser_stats.rendered,asset_browser_stats.tiles,asset_browser_stats.bytes,asset_browser_stats.pending,asset_browser_stats.unavailable);
    studio.designer_asset_thumbnails.shutdown();
    world_scenery_free(scenery);
    dressing.release();
    outcrops.release();
    horizon.release();
    const auto surface_paint_count=studio_surface_gpu.paint.tiles.size(),surface_river_count=studio_surface_gpu.rivers.ranges.size();const auto surface_gpu_bytes=studio_surface_gpu.bytes;
    studio_surface_gpu.release();
    if(qa_item_wind&&!qa_item_wind_passed)launch->exit_code=1;
    if(qa_structure&&!qa_structure_passed)launch->exit_code=1;
    if(qa_editor_npc&&!qa_npc_passed)launch->exit_code=1;
    if(qa_editor_zoom&&!qa_zoom_passed)launch->exit_code=1;
    if(qa_structure_splines&&!structure_spline_qa.passed){structure_spline_qa.check(false,"Bounded runtime ended before structure spline fixture passed");structure_spline_qa.report();launch->exit_code=1;}
    if(qa_editor_long_session&&!long_session_qa.passed){long_session_qa.report();launch->exit_code=1;}
    if(qa_editor_reliability&&!qa_reliability_passed)launch->exit_code=1;
    if(qa_editor_move&&qa_editor_move_phase!=2){std::fprintf(stderr,"[Editor move QA] incomplete phase %u; fast commits %u\n",qa_editor_move_phase,studio.fast_move_commits);launch->exit_code=1;}
    if(qa_landscape_refine&&!qa_refine_passed)launch->exit_code=1;
    if(qa_water_refine&&!qa_water_passed)launch->exit_code=1;
    if(qa_surfaces&&qa_surface_phase!=7)launch->exit_code=1;
    if(qa_live&&qa_live_phase!=(qa_journey?20u:8u))launch->exit_code=1;
    if(qa_preview&&!qa_preview_done)launch->exit_code=1;
    landscape.release();
    const bool showcase_terrain_loaded=showcase_terrain.ready();const auto showcase_terrain_bytes=showcase_terrain.bytes;
    const int showcase_terrain_side=showcase_terrain.side,showcase_terrain_alpha_side=showcase_terrain.alpha_side;
    showcase_terrain.release();
    village_grade.release();
    const auto city_visible_sky_bytes=city_visible_sky.bytes;const bool city_visible_sky_enabled=city_visible_sky.ready();city_visible_sky.release();
    const auto environment_state=world_environment_stats(studio_environment);
    world_environment_free(studio_environment);studio_environment=nullptr;
    world_temporal_free(temporal);if(upscaler)std::printf("[Upscale] frames=%llu\n",world_upscale_frames(upscaler));world_upscale_free(upscaler);std::printf("[Footsteps] owner=%s played=%llu filtered=%llu\n",demo_audio?"demo_audio":"ambience",(unsigned long long)hero_steps_played,(unsigned long long)hero_steps_filtered);world_ambience_free(ambience);world_sky_free(sky);world_occlusion_free(occlusion);world_weather_free(weather);
    world_bloom_free(bloom);world_auto_exposure_free(auto_exposure);
    if(object_motion)std::printf("[Motion] hero object-motion frames=%llu skinned=%llu failures=%llu\n",(unsigned long long)object_motion_frames,(unsigned long long)object_motion_skinned_frames,(unsigned long long)object_motion_failures);
    std::printf("[Foliage prepass] frames=%llu enabled=%d\n",foliage_prepass_frames,int(foliage_depth_prepass));
    far_shadow.collect();
    std::printf("[Far shadow] enabled=%d radius=%.0f renders=%llu gpu_ms_mean=%.3f gpu_ms_max=%.3f\n",int(far_shadow.enabled),far_shadow.radius,(unsigned long long)far_shadow.renders,far_shadow.gpu_samples?far_shadow.gpu_ms_total/far_shadow.gpu_samples:0.,far_shadow.gpu_ms_max);
    far_shadow.release();
    std::printf("[Motion] camp NPC object-motion actors=%llu\n",(unsigned long long)npc_object_motion_actors);
    world_object_motion_free(object_motion);world_scene_particles_free(scene_particles);
    studio.cinematic_audio_shutdown();
    const unsigned recorded_voice_lines=preview_voices.available,recorded_voice_playbacks=preview_voices.playbacks;
    opening_intro_media.unload();
    if(opening_intro_texture)glDeleteTextures(1,&opening_intro_texture);
    opening_cinematic_audio.release();
    preview_voices.release();preview_audio.release();
    if(presentation)ImGui_ImplSDL2_SetGamepadMode(ImGui_ImplSDL2_GamepadMode_Manual,nullptr,0);
    preview_pad.close();controller_test_pad.detach();
    world_demo_audio_free(demo_audio);demo_audio=nullptr;
    terrain.release();
    world_atlas_set_height_edit(nullptr,nullptr);world_atlas_set_water_edit(nullptr,nullptr);studio_document=nullptr;
    world_water_scene_free(water_scene);
    target.release();
    reflection.release();
    const auto local_shadow_bytes=local_light_shadow.bytes();const int local_shadow_side=local_light_shadow.side;
    local_light_shadow.release();
    shadow.release();
    if (profile_gpu) {
        shadow_queries.release();foliage_queries.release();early_opaque_queries.release();queries.release();reflection_queries.release();sky_queries.release();sky_reflection_queries.release();occlusion_queries.release();weather_queries.release();
        terrain_queries.release();water_queries.release();temporal_queries.release();post_queries.release();
    }
    auto texture_state = texture_cache_stats();
    GLenum error = glGetError();
    if(error==GL_NO_ERROR)error=gpu_budget_audit.preexisting_error;
    if(error==GL_NO_ERROR)error=gpu_budget_audit.query_error;
    if (error != GL_NO_ERROR || texture_state.entries || texture_state.references) {
        launch->exit_code = 1;
        std::fprintf(stderr, "[World] Shutdown validation GL=0x%x texture owners=%zu/%zu\n", error, texture_state.entries,
            texture_state.references);
    }

    if (launch->profile[0]||launch->capture[0]) {
        std::string report = std::string(launch->profile[0]?launch->profile:launch->capture) + ".world.json";
        FILE *f = std::fopen(report.c_str(), "w");
        if (f) {
            std::fprintf(f,R"JSON({
  "world_width_m": %.3f, "world_depth_m": %.3f,
  "walk_speed_mps": %.3f, "width_reference_hours": %.3f,
  "startup_ms": %.3f, "terrain_records": %zu, "terrain_gpu_mib": %.3f,
  "terrain_cells": 64, "near_terrain_spacing_m": 2,
  "authored_districts": 3, "authored_instances": %zu, "authored_tree_instances": %zu,
  "authored_prop_instances": %zu, "authored_largest_batch": %zu,
  "temporal_aa": %s, "animated_foliage": true,
  "hemisphere_lighting": true, "shoreline_depth": true, "water_scene_frames": %zu, "water_scene_peak_mib": %.3f,
  "terrain_cpu_peak_mib": %.3f, "generated_tiles": %llu, "cancelled_tiles": %llu, "failed_tiles": %llu,
  "peak_grass_tufts": %zu, "peak_tree_instances": %zu, "peak_scanned_grass_instances": %zu,
  "peak_near_tree_instances": %zu, "peak_mid_tree_instances": %zu,
  "peak_local_tree_cards": %zu, "peak_horizon_tree_cards": %zu, "peak_horizon_cached_roots": %zu,
  "horizon_root_cap": 100000, "horizon_buckets": 8, "horizon_bucket_cap": 3500,
  "reflection_width": %d, "reflection_height": %d,
  "travel_loading_frames": %zu, "longest_travel_loading_streak": %zu,
  "remaining_texture_entries": %zu, "remaining_texture_references": %zu, "gl_error": %u,
  "gpu_timing_note": "render_total_gpu_ms includes all scene passes; shadow_gpu_ms, forest_gpu_ms (main foliage plus early opaque interior) and reflection_gpu_ms are separately timed. Other stage/static-asset fields are unused. Tree counts are submitted instances before alpha/projected-size rejection.",
)JSON",
                world_atlas_width(),world_atlas_depth(),world_atlas_walk_speed(),world_atlas_width()/world_atlas_walk_speed()/3600.,
                startup_ms,metrics.desired,terrain_gpu_bytes/1048576.,
                authored_state.source_instances,authored_state.source_trees,authored_state.source_props,authored_state.largest_batch,
                temporal_enabled?"true":"false",water_scene_frames,water_scene_peak_bytes/1048576.,metrics.peak_cpu_bytes/1048576.,
                (unsigned long long)metrics.generated_total,(unsigned long long)metrics.cancelled_total,(unsigned long long)metrics.failed_total,
                peak_grass,peak_trees,peak_scanned_grass,peak_near,peak_mid,peak_local_cards,peak_horizon,peak_horizon_roots,
                reflection_width,reflection_height,loading_frames,longest_loading_streak,
                texture_state.entries,texture_state.references,unsigned(error));
            std::fprintf(f,R"JSON(  "live_world": {"ready": %s, "frames": %llu, "skeletal_actors": %u, "visible_actors": %u, "rigid_props": %u, "pose_builds": %u, "movement_steps": %llu, "draws": %llu, "physics_bodies": %u, "physics_heap_fallbacks": %llu, "physics_errors": %llu, "gameplay_failed_ticks": %llu},
  "asset_streaming": {"enabled": %s, "resident": %zu, "wanted": %zu, "loading": %zu, "failed": %zu, "critical_pending": %zu, "geometry_budgeted_bytes": %llu, "texture_bytes": %llu, "last_upload_bytes": %llu, "loads": %llu, "evictions": %llu, "cancellations": %llu, "fallback_groups": %zu},
)JSON",studio.components_ready?"true":"false",(unsigned long long)live_system_state.frames,live_animation_state.actors,live_animation_state.visible,live_rigid_count,live_animation_state.pose_builds,(unsigned long long)live_visual_moves,(unsigned long long)live_visual_draws,live_system_state.physics.bodies,(unsigned long long)(live_system_state.physics.total_frame_allocations+live_system_state.physics.query_allocations),(unsigned long long)live_system_state.physics.update_errors,(unsigned long long)live_system_state.failed_gameplay_ticks,live_stream_state.enabled?"true":"false",live_stream_state.resident,live_stream_state.wanted,live_stream_state.loading,live_stream_state.failed,live_stream_state.critical_pending,(unsigned long long)live_stream_state.geometry_bytes,(unsigned long long)live_stream_state.texture_bytes,(unsigned long long)live_stream_state.upload_bytes,(unsigned long long)live_stream_state.loads,(unsigned long long)live_stream_state.evictions,(unsigned long long)live_stream_state.cancellations,live_stream_state.fallback_groups);
            std::fprintf(f,"  \"live_world_qa_phase\": %u,\n",qa_live_phase);
            std::fprintf(f,"  \"playable_preview\": {\"enabled\": %s, \"qa\": %s, \"qa_passed\": %s, \"phase\": %u, \"input_events\": %u, \"walking_steps\": %u, \"blocked_steps\": %u, \"screen\": %d, \"chapter\": %u, \"score_cues\": %u},\n",presentation?"true":"false",qa_preview?"true":"false",qa_preview_done?"true":"false",qa_preview_phase,qa_preview_inputs,qa_preview_walk_steps,qa_preview_blocks,preview_screen,unsigned(preview_chapter),preview_audio.cues);
            std::fprintf(f,"  \"playtest_slice\": {\"enabled\": %s, \"area\": %s, \"section\": %u, \"node\": %zu, \"nodes\": %zu, \"checkpoints\": %zu, \"total_checkpoints\": %zu, \"complete\": %s, \"qa\": %s, \"qa_complete\": %s, \"walked_m\": %.6f, \"blocked_steps\": %llu, \"slope_steps\": %llu, \"walking_frames\": %llu, \"completed_sections\": %u, \"restarts\": %u, \"save_load_checks\": %u},\n",playtest_enabled?"true":"false",world_json::quote(playtest_enabled?playtest_area:"").c_str(),playtest_section,playtest_state.index(),playtest_state.path().nodes.size(),playtest_state.reached(),playtest_state.total(),playtest_state.complete()?"true":"false",playtest_qa?"true":"false",playtest_qa_done?"true":"false",playtest_qa_distance,(unsigned long long)playtest_qa_blocks,(unsigned long long)playtest_qa_slopes,(unsigned long long)playtest_qa_frames,playtest_completed_sections,playtest_restarts,playtest_save_load_checks);
            playtest_combat_qa_report(f);
            grandstone_obstacle_report(f);
            opening_report(f);
            std::fprintf(f,"  \"opening_house_shadows\": {\"frames\": %llu, \"indoor_radius_m\": 20, \"exterior_radius_m\": 160},\n",(unsigned long long)opening_house_shadow_frames);
            opening_prologue_qa_report(f);
            first_journey_qa_report(f);
            std::fprintf(f,"  \"opening_guidance\": {\"camp_frames\": %u, \"journey_frames\": %u, \"stage_mask\": %u, \"derived_from_quest\": true},\n",opening_camp_guidance_frames,first_journey_guidance_frames,first_journey_guidance_stage_mask);
            std::fprintf(f,"  \"opening_experience\": {\"enabled\": %s, \"dialogue_entries\": %llu, \"dialogue_exits\": %llu, \"dialogue_blend_frames\": %llu, \"dialogue_blocked_frames\": %llu, \"cue_starts\": %u, \"duplicate_cues_suppressed\": %u, \"muted_cues_suppressed\": %u, \"notice_events\": [%u,%u,%u,%u,%u,%u], \"notice_frames\": %u, \"context_hint_frames\": %u, \"loot_prompt_suppressed_frames\": %u, \"camera_preserved_starts\": [%u,%u], \"camera_preserved_retries\": [%u,%u], \"render_camera_only\": true},\n",
                opening_enabled?"true":"false",(unsigned long long)opening_dialogue_blend.entries,(unsigned long long)opening_dialogue_blend.exits,(unsigned long long)opening_dialogue_blend.blend_frames,(unsigned long long)opening_dialogue_blend.blocked_frames,
                preview_audio.cues,preview_audio.duplicate_cues,preview_audio.muted_cues,first_journey_notice_events[0],first_journey_notice_events[1],first_journey_notice_events[2],first_journey_notice_events[3],first_journey_notice_events[4],first_journey_notice_events[5],first_journey_notice_frames,first_journey_context_hint_frames,first_journey_loot_prompt_suppressed_frames,
                first_journey_camera_preserved_starts[0],first_journey_camera_preserved_starts[1],first_journey_camera_preserved_retries[0],first_journey_camera_preserved_retries[1]);
            std::fprintf(f,"  \"first_journey\": {\"enabled\": %s, \"stage\": %s, \"camp_mask\": %u, \"loaned\": %s, \"supplies\": %s, \"rewarded\": %s, \"complete\": %s, \"saves\": %u, \"restores\": %u, \"coins\": %u, \"food\": %u, \"reputation\": %u, \"live_wins\": %u, \"live_losses\": %u, \"enemy_steps\": %u, \"reward_health_before\": %.3f, \"reward_health_after\": %.3f},\n",first_journey_enabled?"true":"false",world_json::quote(world_first_journey::stage_name(first_journey_state.stage())).c_str(),first_journey_state.camp_spoken(),first_journey_state.weapon_unlocked()?"true":"false",first_journey_state.supplies_recovered()?"true":"false",first_journey_state.reward_granted()?"true":"false",first_journey_state.town_reached()?"true":"false",first_journey_saves,first_journey_restores,first_journey_frame.coins,first_journey_frame.food,first_journey_frame.reputation,first_journey_live_wins,first_journey_live_losses,first_journey_enemy_steps,first_journey_reward_health_before,first_journey_reward_health_after);
            std::fprintf(f,"  \"opening_interior\": {\"enabled\": %s, \"probe_domains\": %zu, \"release_city_fallbacks\": %s, \"city_suspended\": %s, \"city_groups\": %zu, \"policy_changes\": %llu},\n",
                opening_interior.enabled?"true":"false",opening_interior.probes.size(),opening_interior.release_city_fallbacks?"true":"false",
                live_stream_state.interior_city_suspended?"true":"false",live_stream_state.interior_city_groups,(unsigned long long)live_stream_state.interior_policy_changes);
            std::fprintf(f,"  \"playtest_combat\": {\"ready\": %s, \"starts\": %u, \"wins\": %u, \"defeats\": %u, \"hero_draw_frames\": %llu, \"enemy_visible_frames\": %llu, \"start_x\": %.6f, \"start_z\": %.6f, \"enemy_x\": %.6f, \"enemy_z\": %.6f, \"feet\": %.6f, \"error\": %s},\n",playtest_combat_ready?"true":"false",playtest_combat_starts,playtest_combat_wins,playtest_combat_defeats,(unsigned long long)playtest_hero_draw_frames,(unsigned long long)playtest_enemy_visible_frames,playtest_combat_start_x,playtest_combat_start_z,playtest_combat_enemy_x,playtest_combat_enemy_z,double(playtest_combat_feet),world_json::quote(playtest_combat_error).c_str());
            std::fprintf(f,"  \"view_clip\": {\"near_plane\": %.4f, \"far_plane\": 15000, \"imported_walk_geometry\": %s},\n",view_near_plane(),active_playtest_walk?"true":"false");
            std::fprintf(f,"  \"playtest_input\": {\"enabled\": %s, \"passed\": %s, \"phase\": %u, \"events\": %u, \"cases\": %u, \"synthetic_completion_for_travel_control_only\": %s},\n",playtest_input_qa?"true":"false",playtest_input_done?"true":"false",playtest_input_phase,playtest_input_events,playtest_input_cases,playtest_input_synthetic_completion?"true":"false");
            std::fprintf(f,"  \"controller\": {\"qa\": %s, \"qa_passed\": %s, \"phase\": %u, \"assertions\": %u, \"connects\": %u, \"disconnects\": %u, \"rumble_requests\": %u},\n",qa_controller?"true":"false",controller_qa_passed?"true":"false",controller_qa_phase,controller_qa_checks,preview_pad.connects,preview_pad.disconnects,preview_pad.rumble_requests);
            sword_stick_qa_report(f);
            walking_controls_report(f);
            arena_qa_report(f);
            severing_report(f);
            std::fprintf(f,"  \"arena_render\": {\"draws\": %llu, \"bow_draws\": %llu, \"arrow_draws\": %llu, \"bow_pose_assets\": %u, \"failures\": %u},\n",(unsigned long long)arena_render_state.draws,(unsigned long long)arena_render_state.bow_draws,(unsigned long long)arena_render_state.arrow_draws,arena_render_state.bow_pose_assets,arena_render_state.failures);
            std::fprintf(f,"  \"milestone_review\": {\"enabled\": %s, \"view\": %d, \"portrait_frames\": %llu, \"focus_m\": %.4f, \"player_body\": %s},\n",milestone_finish?"true":"false",int(milestone_review.view),milestone_review.portrait_frames,portrait_focus_distance,milestone_review.portrait()&&third_person?"true":"false");
            std::fprintf(f,"  \"gpu_stage_timing_available\": %s,\n",profile_gpu?"true":"false");
            std::fprintf(f,"  \"trail_camp\": {\"active\": %s, \"actors\": %u, \"walking\": %u, \"seated\": %u, \"visible\": %u, \"pose_assets\": %u, \"failures\": %llu, \"unsafe_roots\": %llu},\n",trail_camp_state.active?"true":"false",trail_camp_state.actors,trail_camp_state.walking,trail_camp_state.seated,trail_camp_state.visible,trail_camp_state.pose_assets,(unsigned long long)trail_camp_state.failures,(unsigned long long)trail_camp_state.unsafe_roots);
            std::fprintf(f,"  \"arrival_camp_render\": {\"active\": %s, \"actors\": %u, \"walking\": %u, \"seated\": %u, \"visible\": %u, \"pose_assets\": %u, \"failures\": %llu, \"unsafe_roots\": %llu},\n",arrival_camp_state.active?"true":"false",arrival_camp_state.actors,arrival_camp_state.walking,arrival_camp_state.seated,arrival_camp_state.visible,arrival_camp_state.pose_assets,(unsigned long long)arrival_camp_state.failures,(unsigned long long)arrival_camp_state.unsafe_roots);
            auto report_people=[&](const char *name,const WorldTrailCampStats &s){
                std::fprintf(f,"  \"%s\": {\"attention_frames\": %llu, \"speaking_frames\": %llu, \"listening_frames\": %llu, \"acknowledgments\": %llu, \"head_uploads\": %llu, \"cpu_bytes\": %llu, \"gpu_bytes\": %llu, \"failures\": %llu, \"unsafe_roots\": %llu},\n",name,
                    (unsigned long long)s.attention_frames,(unsigned long long)s.speaking_frames,(unsigned long long)s.listening_frames,(unsigned long long)s.acknowledgments,(unsigned long long)s.head_uploads,
                    (unsigned long long)s.presentation_cpu_bytes,(unsigned long long)s.presentation_gpu_bytes,(unsigned long long)s.failures,(unsigned long long)s.unsafe_roots);
            };
            report_people("arrival_camp_presentation",arrival_camp_state);
            report_people("town_people_presentation",first_journey_town_state);
            std::fprintf(f,"  \"presentation\": {\"swap_interval\": %d, \"vsync_unchanged\": %s, \"completion_policy\": %s, \"policy_enabled\": %s, \"complete_before_swap\": %s, \"completed_frames\": %llu, \"roost_completion_frames\": %llu, \"mean_completion_wait_ms\": %.6f, \"wait_in_frame_intervals\": true},\n",actual_swap_interval,actual_swap_interval==initial_swap_interval?"true":"false",world_json::quote(presentation?"vsync_cpu_gpu_overlap":"village_and_roosts").c_str(),
                complete_before_swap?"true":"false",completed_presentation_frames?"true":"false",(unsigned long long)completed_presentation_frames,
                (unsigned long long)roost_completion_frames,
                completion_wait_ms/std::max(uint64_t(1),completed_presentation_frames));
            std::fprintf(f,"  \"trail_camp_audit_samples\": %u,\n",trail_camp_audit_samples);
            std::fprintf(f,"  \"trail_camp_path_audit_samples\": %u,\n",trail_camp_path_audit_samples);
            std::fprintf(f,"  \"adaptive_gpu_timing\": {\"automatic_mode_only\": true, \"begun_frames\": %llu},\n",
                (unsigned long long)adaptive_queries.begun_frames);
            std::fprintf(f,R"JSON(  "volumetric_clouds": %s, "sky_target_mib": %.6f, "sky_noise_mib": %.6f,
  "sky_target_resizes": %llu, "sky_draw_calls": %llu, "sky_view_steps": %u,
  "contact_lighting": %s, "indirect_target_mib": %.6f, "occlusion_mib": %.6f,
  "occlusion_frames": %llu, "occlusion_resizes": %llu, "occlusion_failures": %zu,
  "mean_sky_gpu_ms": %.6f, "mean_contact_gpu_ms": %.6f,
)JSON",graphics.clouds&&sky_state.draw_calls?"true":"false",sky_state.target_bytes/1048576.,sky_state.noise_bytes/1048576.,
                (unsigned long long)sky_state.target_resizes,(unsigned long long)sky_state.draw_calls,sky_state.view_steps,
                contact_enabled&&graphics.contact_shadows?"true":"false",indirect_target_bytes/1048576.,occlusion_state.bytes/1048576.,
                (unsigned long long)occlusion_state.resolves,(unsigned long long)occlusion_state.resizes,occlusion_failures,
                sky_gpu_sum/std::max(size_t(1),lighting_gpu_samples),occlusion_gpu_sum/std::max(size_t(1),lighting_gpu_samples));
            std::fprintf(f,"  \"shadow_mesh_lod\": %s, \"procedural_shadow_proxy_groups\": %zu, \"authored_shadow_proxy_groups\": %zu, \"procedural_shadow_proxies_synced\": %s,\n",
                shadow_mesh_lod?"true":"false",procedural_shadow_proxies,authored_state.shadow_proxy_groups,shadow_proxies_synced?"true":"false");
            std::fprintf(f,"  \"authored_shadow_clip\": {\"enabled\":%s,\"tested\":%llu,\"submitted\":%llu,\"rejected\":%llu,\"fail_open\":%llu,\"upload_failures\":%llu},\n",
                authored_state.shadow_clip_enabled?"true":"false",(unsigned long long)authored_state.shadow_clip_tested,
                (unsigned long long)authored_state.shadow_clip_submitted,(unsigned long long)authored_state.shadow_clip_rejected,
                (unsigned long long)authored_state.shadow_clip_fail_open,(unsigned long long)authored_state.shadow_clip_upload_failures);
            std::fprintf(f,"  \"graphics\": {\"preset\": %s, \"automatic\": %s, \"target_fps\": %d, \"display_width\": %d, \"display_height\": %d, \"render_width\": %d, \"render_height\": %d, \"effective_scale\": %.3f, \"minimum_observed_scale\": %.3f, \"maximum_observed_scale\": %.3f, \"scale_changes\": %llu, \"accepted_gpu_samples\": %llu, \"excluded_gpu_samples\": %llu, \"stale_gpu_samples\": %llu, \"apply_failures\": %llu, \"applied_changes\": %llu, \"reflections\": %s, \"reflection_scale\": %.2f, \"contact_shadows\": %s, \"volumetric_fog\": %s, \"volumetric_clouds\": %s, \"floor_limited\": %s, \"cpu_limited\": %s},\n",
                world_json::quote(world_graphics_preset_name(graphics.preset)).c_str(),graphics.auto_quality?"true":"false",graphics.target_fps,display_w,display_h,graphics_render_w,graphics_render_h,
                float(graphics_render_w)/std::max(1,display_w),minimum_rendered_scale,maximum_rendered_scale,(unsigned long long)quality_reallocations,(unsigned long long)quality_state.accepted_samples,
                (unsigned long long)quality_state.excluded_samples,(unsigned long long)quality_state.stale_samples,(unsigned long long)quality_apply_failures,(unsigned long long)graphics_apply_count,
                graphics.reflections?"true":"false",graphics.reflection_scale,graphics.contact_shadows?"true":"false",graphics.volumetric_fog?"true":"false",
                graphics.clouds?"true":"false",quality_state.floor_limited?"true":"false",quality_state.cpu_limited?"true":"false");
            const auto environment_profile=world_environment_effects_profile(graphics.environment_effects);
            std::fprintf(f,"  \"environment_effects\": {\"level\":%s,\"smoke_budget\":%u,\"ambient_budget\":%u,\"fog_steps\":%d,\"npc_motion_actors\":%llu},\n",world_json::quote(world_environment_effects_name(graphics.environment_effects)).c_str(),environment_profile.smoke_particles,environment_profile.ambient_particles,environment_profile.fog_steps,(unsigned long long)npc_object_motion_actors);
            const auto water_profile=world_water_quality_profile(graphics.water_quality);
            std::fprintf(f,"  \"water_quality\": {\"level\":%s,\"planar_enabled\":%s,\"mirror_scale_cap\":%.2f,\"ssr_steps\":%d,\"ssr_refine_steps\":%d,\"filtered_reflections\":%s,\"mirror_width\":%d,\"mirror_height\":%d,\"swimming_physics_unchanged\":true},\n",world_json::quote(world_water_quality_name(graphics.water_quality)).c_str(),graphics.reflections&&water_profile.planar?"true":"false",water_profile.mirror_scale_cap,graphics.reflections?water_profile.ssr_steps:0,water_profile.ssr_refine_steps,water_profile.filtered?"true":"false",reflection.w,reflection.h);
            std::fprintf(f,"  \"foliage_specialization\": {\"linked\": %s, \"main_color_batches\": %llu, \"main_color_instances\": %llu, \"scope\": \"plain_generated_grass_only\", \"formulas_shared\": true},\n",
                generated_grass_shader_linked?"true":"false",(unsigned long long)dressing.specialized_batches,(unsigned long long)dressing.specialized_instances);
            const auto vegetation_budget=world_ground_cover_quality::profile(graphics.foliage_density);
            std::fprintf(f,"  \"vegetation_budgets\": {\"grass_roots\": %zu, \"fern_roots\": %zu, \"tree_roots\": %zu, \"rock_roots\": %zu, \"grass_blade_keep\": %.3f, \"source_record_capacity\": %u},\n",vegetation_budget.grass,vegetation_budget.ferns,vegetation_budget.trees,vegetation_budget.rocks,vegetation_budget.grass_blade_keep,WORLD_SOURCE_RECORD_LIMIT);
            std::fprintf(f,"  \"performance_settings\": {\"frame_limit\": %d, \"effective_target_fps\": %d, \"foliage_density\": %s, \"density_scale\": %.3f, \"authored_density_candidates\": %zu, \"authored_density_rejected\": %zu, \"scanned_near_instances\": %zu, \"scanned_mid_instances\": %zu, \"generated_density_rejected\": %llu},\n",
                graphics.frame_rate_limit,world_graphics_effective_target_fps(graphics,graphics_refresh_hz(window)),world_json::quote(world_foliage_density_name(graphics.foliage_density)).c_str(),foliage_density_scale,
                authored_state.foliage_density_candidates,authored_state.foliage_density_rejected,dressing.scanned_near_count,dressing.scanned_mid_count,(unsigned long long)dressing.density_rejected);
            std::fprintf(f,"  \"designer_viewport\": {\"open\": %s, \"logical_width\": %d, \"logical_height\": %d, \"x\": %.3f, \"y\": %.3f, \"width\": %.3f, \"height\": %.3f, \"drawable_x\": %d, \"drawable_y\": %d, \"drawable_width\": %d, \"drawable_height\": %d, \"scene_width\": %d, \"scene_height\": %d, \"aspect\": %.6f, \"separate_camera\": %s},\n",
                studio.open?"true":"false",logical_w,logical_h,designer_view.x,designer_view.y,designer_view.w,designer_view.h,designer_drawable.x,designer_drawable.y,designer_drawable.w,designer_drawable.h,graphics_render_w,graphics_render_h,cam.aspect,designer_cameras.active?"true":"false");
            std::fprintf(f,"  \"hdr_bloom\": {\"enabled\": %s, \"strength\": %.6f, \"startup_ready\": %s, \"frames\": %zu, \"width\": %d, \"height\": %d, \"bytes\": %llu, \"allocations\": %llu, \"resolves\": %llu, \"failures\": %zu, \"scene_linear\": true, \"post_timing_includes_bloom\": true},\n",
                bloom_enabled?"true":"false",bloom_strength,bloom_startup_ready?"true":"false",bloom_play_frames,
                bloom_state.quarter_width,bloom_state.quarter_height,(unsigned long long)bloom_state.bytes,
                (unsigned long long)bloom_state.allocations,(unsigned long long)bloom_state.resolves,bloom_failures);
            unsigned minimap_line_counts[3]={};for(unsigned i=0;i<minimap_geometry.count;++i)++minimap_line_counts[unsigned(minimap_geometry.lines[i].kind)];
            std::fprintf(f,"  \"minimap\": {\"config_loaded\": %s, \"config_valid\": %s, \"enabled\": %s, \"visible\": %s, \"visible_frames\": %llu, \"hidden_frames\": %llu, \"radius_m\": %.6f, \"landmarks\": %u, \"cache_builds\": %llu, \"cache_failures\": %llu, \"line_count\": %u, \"river_lines\": %u, \"road_lines\": %u, \"bridge_lines\": %u, \"cache_truncated\": %u, \"peak_primitives\": %u, \"primitive_limit\": 512, \"primitive_scope\": \"bounded ImGui draw primitives\", \"budget_drops\": %u, \"npc_dots\": %u, \"poi_labels\": %u, \"player\": [%.6f,%.6f,%.6f], \"yaw\": %.9f, \"north_up\": true, \"input_window\": false, \"normal_gpu_readbacks\": 0, \"qa\": %s, \"qa_passed\": %s, \"qa_phase\": %u},\n",
                minimap_present?"true":"false",minimap_valid?"true":"false",minimap_valid&&minimap_config.enabled?"true":"false",minimap_drawn?"true":"false",(unsigned long long)minimap_stats.visible_frames,(unsigned long long)minimap_stats.hidden_frames,minimap_config.radius,minimap_config.count,(unsigned long long)minimap_stats.cache_builds,(unsigned long long)minimap_stats.cache_failures,minimap_geometry.count,minimap_line_counts[0],minimap_line_counts[1],minimap_line_counts[2],minimap_geometry.truncated,minimap_stats.peak_primitives,minimap_stats.dropped,minimap_stats.npc_dots,minimap_stats.poi_labels,minimap_stats.player_x,minimap_stats.player_y,minimap_stats.player_z,minimap_stats.yaw,qa_minimap?"true":"false",minimap_qa_passed?"true":"false",minimap_qa_phase);
            std::fprintf(f,"  \"grass_interaction\": {\"enabled\": true, \"owner\": \"gameplay_player_feet\", \"active_frames\": %llu, \"moving_frames\": %llu, \"history_samples\": %llu, \"recovery_frames\": %llu, \"clears\": %llu, \"travel_m\": %.6f, \"max_step_m\": %.6f, \"peak_contacts\": %u, \"active_history\": %u, \"feet\": [%.6f,%.6f,%.6f], \"radius_m\": %.6f, \"broadphase_squared_m\": %.6f, \"qa\": %s, \"qa_passed\": %s, \"qa_phase\": %u, \"fixed_contact_capacity\": 4, \"global_density_ranges_unchanged\": true},\n",
                (unsigned long long)grass_interaction.active_frames,(unsigned long long)grass_interaction.moving_frames,
                (unsigned long long)grass_interaction.history_samples,(unsigned long long)grass_interaction.recovery_frames,
                (unsigned long long)grass_interaction.clears,grass_interaction.travel,grass_interaction.max_step,
                grass_interaction.peak_contacts,grass_interaction.active_history,grass_interaction.last_x,grass_interaction.last_y,grass_interaction.last_z,
                visual_grass_interactor[3],grass_interaction.bounds[2],qa_grass?"true":"false",grass_qa_passed?"true":"false",grass_qa_phase);
            std::fprintf(f,"  \"authored_depth_first\": {\"enabled\": %s, \"frames\": %zu, \"same_geometry\": true, \"same_materials\": true, \"single_submission\": true},\n",
                authored_first_enabled?"true":"false",authored_first_frames);
            std::fprintf(f,"  \"water_reflection_demand\": {\"explicit_disabled_ocean_only\": true, \"default_legacy_unchanged\": true, \"force_legacy_qa\": %s, \"legacy_ocean_candidate_frames\": %llu, \"ocean_disabled_frames\": %llu, \"local_consumer_frames\": %llu, \"skipped_unused_ocean_mirrors\": %llu, \"rendered_mirrors\": %llu, \"river_planar_sampling\": true, \"river_reflected_draws\": %llu, \"shared_mirror_targets\": 1},\n",
                force_legacy_ocean_reflection?"true":"false",(unsigned long long)ocean_legacy_candidate_frames,
                (unsigned long long)ocean_disabled_frames,(unsigned long long)local_mirror_consumer_frames,
                (unsigned long long)unused_ocean_mirror_skips,(unsigned long long)water_mirror_rendered_frames,(unsigned long long)studio_river_reflection_draws);
            std::fprintf(f,"  \"source_camera_collision\": {\"casts\": %llu, \"blocked\": %llu, \"query_failures\": %llu, \"ordinary_city_enabled\": true},\n",
                (unsigned long long)source_camera_casts,(unsigned long long)source_camera_blocks,(unsigned long long)source_camera_query_failures);
            std::fprintf(f,"  \"destination_readiness\": {\"overlapping_reasons\": true, \"probe_ms\": %.3f, \"material_ms\": %.3f, \"reasons\": {",
                destination_probe_ms,destination_material_ms);
            const char *wait_names[]={"terrain","critical_assets","horizon","near_cover","tree_cover","wildlife","bandits","trail_camp","arrival_camp","dragons"};
            for(unsigned i=0;i<10;++i)std::fprintf(f,"%s\"%s\": {\"frames\": %llu, \"milliseconds\": %.3f}",i?",":"",wait_names[i],(unsigned long long)destination_wait_frames[i],destination_wait_ms[i]);
            std::fprintf(f,"}},\n");
            std::fprintf(f,"  \"early_gpu_submission\": {\"enabled\": true, \"frames\": %zu, \"waits_for_completion\": false},\n",early_gpu_submitted_frames);
            std::fprintf(f,"  \"main_material_preparation\": {\"ready\": %s, \"calls\": %u, \"passes\": %u, \"startup_calls\": %u, \"destination_calls\": %u, \"failures\": %u, \"milliseconds\": %.3f, \"terrain_passes\": %u, \"static_model_passes\": %u, \"procedural_grass_passes\": %u, \"scanned_tuft_passes\": %u, \"near_tree_passes\": %u, \"pose_passes\": %u, \"headings_per_call\": 4, \"main_color_attachments\": 2, \"loading_only\": true, \"walking_fences\": 0, \"temporal_history_unchanged\": true},\n",
                main_material_warm.ready?"true":"false",main_material_warm.calls,main_material_warm.passes,
                main_material_warm.startup_calls,main_material_warm.destination_calls,main_material_warm.failures,main_material_warm.milliseconds,
                main_material_warm.terrain_passes,main_material_warm.static_passes,main_material_warm.grass_passes,
                main_material_warm.scanned_passes,main_material_warm.near_tree_passes,main_material_warm.pose_passes);
            std::fprintf(f,"  \"demo_tour\": {\"steps\": %u, \"qa_integration\": %s},\n",demo_tour_steps,qa_demo_tour?"true":"false");
            std::fprintf(f,"  \"atlas_map\": {\"loaded\": %s, \"attempts\": %u, \"visible\": %s, \"tools_visible\": %s, \"qa_events\": %u, \"lazy\": true},\n",
                atlas_map_loaded?"true":"false",atlas_map_attempted?1u:0u,show_map?"true":"false",show_tools?"true":"false",atlas_map_qa_events);
            std::fprintf(f,"  \"structural_collision\": {\"qa\":%s,\"passed\":%s,\"phase\":%u,\"checks\":%u,\"enabled\":%s,\"source_fingerprint\":\"%016llx\",\"partitions\":%zu,\"async_started\":%u,\"commits\":%u,\"failures\":%u,\"last_cook_ms\":%.3f},\n",qa_structure?"true":"false",qa_structure_passed?"true":"false",qa_structure_phase,qa_structure_checks,active_structure_world?"true":"false",(unsigned long long)structure_live_fingerprint,structure_support?structure_support->partitions():0,structure_async_started,structure_async_commits,structure_async_failures,structure_last_ms);
            std::fprintf(f,"  \"item_wind\": {\"qa\":%s,\"passed\":%s,\"phase\":%u,\"checks\":%u},\n",qa_item_wind?"true":"false",qa_item_wind_passed?"true":"false",qa_item_wind_phase,qa_item_wind_checks);
            std::fprintf(f,"  \"npc_paths\": {\"qa\":%s,\"passed\":%s,\"phase\":%u,\"checks\":%u},\n",qa_editor_npc?"true":"false",qa_npc_passed?"true":"false",qa_npc_phase,qa_npc_checks);
            std::fprintf(f,"  \"editor_reliability\": {\"qa\":%s,\"passed\":%s,\"phase\":%u,\"checks\":%u,\"structural_rejections\":%u,\"generated_trunks\":%zu,\"generated_rocks\":%zu,\"collision_ready\":%s},\n",qa_editor_reliability?"true":"false",qa_reliability_passed?"true":"false",qa_reliability_phase,qa_reliability_checks,studio.structure_guard_rejections,generated_cover_collision.obstacles(world_cover_collision::Kind::Trunk).size(),generated_cover_collision.obstacles(world_cover_collision::Kind::Rock).size(),generated_cover_collision_ready?"true":"false");
            std::fprintf(f,"  \"editor_zoom\": {\"qa\":%s,\"passed\":%s,\"phase\":%u,\"checks\":%u,\"dolly_events\":%u,\"speed_events\":%u},\n",qa_editor_zoom?"true":"false",qa_zoom_passed?"true":"false",qa_zoom_phase,qa_zoom_checks,editor_dolly_events,editor_speed_events);
            std::fprintf(f,"  \"structure_splines\": {\"qa\":%s,\"passed\":%s,\"phase\":%u,\"checks\":%u,\"walls\":%u,\"bridges\":%u,\"segments\":%u,\"triangles\":%llu,\"gpu_bytes\":%llu,\"draws\":%u,\"shadow_draws\":%u},\n",qa_structure_splines?"true":"false",structure_spline_qa.passed?"true":"false",structure_spline_qa.phase,structure_spline_qa.checks,structure_gpu_walls,structure_gpu_bridges,structure_gpu_segments,(unsigned long long)structure_gpu_triangles,(unsigned long long)structure_gpu_bytes,structure_gpu_draws,structure_gpu_shadow_draws);
            std::fprintf(f,"  \"editor_long_session\": {\"qa\":%s,\"passed\":%s,\"phase\":%u,\"checks\":%u,\"edits\":%u,\"undos\":%u,\"redos\":%u},\n",qa_editor_long_session?"true":"false",long_session_qa.passed?"true":"false",long_session_qa.phase,long_session_qa.checks,long_session_qa.edits,long_session_qa.undos,long_session_qa.redos);
            std::fprintf(f,"  \"terrain_apply_history\": {\"sample_limit\":%zu,\"retained_samples\":%zu,\"total_commits\":%llu,\"dropped_samples\":%llu},\n",editor_apply_perf_limit,editor_apply_perf.size(),(unsigned long long)editor_apply_perf_total,(unsigned long long)(editor_apply_perf_total-editor_apply_perf.size()));
            std::fprintf(f,"  \"terrain_apply_performance\": [");
            for(size_t i=0;i<editor_apply_perf.size();++i){const auto &p=editor_apply_perf[i];std::fprintf(f,"%s{\"commit\":%.0f,\"main_thread_ms\":%.3f,\"rebuilt_tiles\":%.0f,\"masks_rebuilt\":%.0f,\"visible_ms\":%.3f}",i?",":"",p[0],p[1],p[2],p[3],p[4]);}
            std::fprintf(f,"],\n");
            std::fprintf(f,"  \"studio\": {\"applied\": %u, \"rejected\": %u, \"qa_phase\": %u, \"terrain_tiles\": %zu, \"added_models\": %zu, \"lights\": %zu, \"foliage_masks\": %zu, \"materials\": %zu, \"decals\": %zu, \"splines\": %zu, \"reused_model_groups\": %zu},\n",studio.applied,studio.rejected,qa_studio_phase,studio.live.terrain.size(),studio.live.placements.size(),studio.live.lights.size(),studio.live.stamps.size(),studio.live.materials.size(),studio.live.decals.size(),studio.live.splines.size(),authored_state.reused_groups);
            std::fprintf(f,"  \"source_terrain_materials\": {\"requested\": %s, \"enabled\": %s, \"layers\": %u, \"arrays\": %u, \"tile_side\": %d, \"weight_side\": %d, \"gpu_bytes\": %llu, \"source_uv_metres\": true, \"source_alpha_channels\": true},\n",
                showcase_terrain_requested?"true":"false",showcase_terrain_loaded?"true":"false",showcase_terrain_loaded?8u:0u,showcase_terrain_loaded?3u:0u,
                showcase_terrain_side,showcase_terrain_alpha_side,(unsigned long long)showcase_terrain_bytes);
            std::fprintf(f,"  \"surface_editors\": {\"phase\": %u, \"images\": %zu, \"layers\": %zu, \"paint_tiles\": %zu, \"projections\": %zu, \"winds\": %zu, \"paths\": %zu, \"river_meshes\": %zu, \"gpu_bytes\": %llu},\n",qa_surface_phase,studio.live.surfaces.images.size(),studio.live.surfaces.layers.size(),surface_paint_count,studio.live.surfaces.projections.size(),studio.live.surfaces.winds.size(),studio.live.surfaces.ribbons.size(),surface_river_count,(unsigned long long)surface_gpu_bytes);
            std::fprintf(f,"  \"runtime_editors\": {\"phase\": %u, \"commits\": %u, \"actors\": %zu, \"navigation_areas\": %zu, \"particles\": %zu, \"waters\": %zu, \"graphs\": %zu, \"graph_button_compiles\": %u},\n",qa_runtime_phase,studio.runtime_commits,studio.live.actors.size(),studio.live.navigation.size(),studio.live.particles.size(),studio.live.waters.size(),studio.live.graphs.size(),studio.graph_compiles);
            const auto &editor_index=studio.designer_scene_index.counters();
            std::fprintf(f,"  \"editor_optimization\": {\"source_filter_rebuilds\": %llu, \"authored_row_rebuilds\": %llu, \"topology_rebuilds\": %llu, \"inspector_refreshes\": %llu, \"legacy_baseline_copies\": %llu, \"pick_index_rebuilds\": %u, \"last_pick_exact_tests\": %u, \"last_pick_nodes\": %u, \"pick_mesh_cache_hits\": %llu, \"pick_mesh_cache_misses\": %llu},\n",
                (unsigned long long)editor_index.source_filter_rebuilds,(unsigned long long)editor_index.authored_row_rebuilds,(unsigned long long)editor_index.topology_rebuilds,(unsigned long long)studio.designer_inspector_refreshes,(unsigned long long)studio.designer_baseline_copies,studio.picking_index_rebuilds,studio.picking_exact_checks,studio.picking_index_nodes,(unsigned long long)studio.picking_cache_hits,(unsigned long long)studio.picking_cache_misses);
            std::fprintf(f,"  \"landscape_editor\": {\"qa\": %s, \"passed\": %s, \"phase\": %u, \"checks\": %u, \"ecology_areas\": %zu, \"terrain_tiles\": %zu, \"strokes\": %u, \"cancelled\": %u, \"samples\": %u, \"rivers_created\": %u, \"scanned_grass\": %zu, \"tree_roots\": %zu},\n",qa_landscape?"true":"false",qa_landscape_passed?"true":"false",qa_landscape_phase,qa_landscape_checks,studio.live.ecology.size(),studio.live.terrain.size(),studio.landscape_committed_strokes,studio.landscape_cancelled_strokes,studio.landscape_brush_samples,studio.landscape_rivers_created,dressing.scanned_grass_count,dressing.tree_count);
            std::fprintf(f,"  \"landscape_refine\": {\"qa\":%s,\"passed\":%s,\"phase\":%u,\"checks\":%u,\"pond_commits\":%u,\"river_point_commits\":%u,\"generated_collision_ready\":%s},\n",
                qa_landscape_refine?"true":"false",qa_refine_passed?"true":"false",qa_refine_phase,qa_refine_checks,
                studio.water_geometry_commits,studio.surface_point_commits,generated_cover_collision_ready?"true":"false");
            std::fprintf(f,"  \"water_refine\": {\"qa\":%s,\"passed\":%s,\"phase\":%u,\"checks\":%u,\"joined_level\":%.9f},\n",qa_water_refine?"true":"false",qa_water_passed?"true":"false",qa_water_phase,qa_water_checks,qa_water_joined_level);
            std::fprintf(f,"  \"water_authoring_draws\": {\"ponds\": [");
            for(unsigned i=0;i<studio_pond_draws.size();++i)std::fprintf(f,"%s%llu",i?", ":"",(unsigned long long)studio_pond_draws[i]);
            std::fprintf(f,"], \"pond_reflections\": [");
            for(unsigned i=0;i<studio_pond_reflection_draws.size();++i)std::fprintf(f,"%s%llu",i?", ":"",(unsigned long long)studio_pond_reflection_draws[i]);
            std::fprintf(f,"], \"rivers\": %llu},\n",(unsigned long long)studio_river_draws);
            std::fprintf(f,"  \"production_editor\": {\"active_tool\": %d, \"entities\": %zu, \"id_highwater\": %llu, \"project_saved\": %s, \"qa_enabled\": %s, \"qa_phase\": %u, \"qa_checks\": %u, \"qa_failed\": %s},\n",studio.tab,studio.live.entities.nodes.size(),(unsigned long long)studio.production_id_highwater,studio.production_project_saved?"true":"false",qa_production?"true":"false",qa_production_phase,qa_production_checks,qa_production_failed?"true":"false");
            std::fprintf(f,"  \"city_visible_sky\": {\"enabled\": %s, \"gpu_bytes\": %llu, \"material_ibl_unchanged\": true, \"local_probe_background_unchanged\": true},\n",city_visible_sky_enabled?"true":"false",(unsigned long long)city_visible_sky_bytes);
            std::fprintf(f,"  \"studio_tools\": {\"document_version\": 5, \"groups\": %zu, \"asset_definitions\": %zu, \"gizmo_drags\": %u, \"gizmo_finishes\": %u, \"mesh_pick_requests\": %u, \"hdr_enabled\": %s, \"hdr_side\": %u, \"hdr_levels\": %u, \"hdr_gpu_bytes\": %llu},\n",studio.live.groups.size(),studio.live.assets.size(),studio.gizmo_drags,studio.gizmo_finishes,studio.pick_requests,studio.live.environment.enabled?"true":"false",environment_state.side,environment_state.levels,(unsigned long long)environment_state.bytes);
            const double draft_dx=studio.draft.placements.empty()||studio.live.placements.empty()?0:studio.draft.placements.front().x-studio.live.placements.front().x;
            std::fprintf(f,"  \"studio_input\": {\"mesh_hits\": %u, \"ui_undos\": %u, \"draft_first_delta_x\": %.8f, \"history_back\": %zu, \"history_forward\": %zu},\n",studio.mesh_hits,studio.ui_undos,draft_dx,studio.history.back.size(),studio.history.forward.size());
            std::fprintf(f,"  \"local_probes\": {\"enabled\": %s, \"cache_regions\": %u, \"count\": %u, \"ready\": %u, \"side\": 128, \"bytes\": %llu, \"capture_calls\": %llu, \"faces\": %llu, \"readbacks\": %llu, \"bind_calls\": %llu, \"failures\": %llu, \"loading_bakes\": %u, \"walking_bakes\": 0, \"ordinary_mip_reflections\": false, \"ggx_filtered_reflections\": true, \"sh9_diffuse\": true},\n",
                local_probes_enabled?"true":"false",local_probe_cache_regions,local_probe_state.count,local_probe_state.ready,
                (unsigned long long)local_probe_state.bytes,(unsigned long long)local_probe_state.capture_calls,
                (unsigned long long)local_probe_state.capture_faces,(unsigned long long)local_probe_state.readbacks,
                (unsigned long long)local_probe_state.bind_calls,(unsigned long long)local_probe_state.failures,local_probe_loading_bakes);
            std::fprintf(f,"  \"stronghold\": {\"enabled\": %s, \"actors\": %u, \"families\": 4, \"walking\": %u, \"visible\": %u, \"peak_visible\": %u, \"pose_assets\": %u, \"updates\": %llu, \"uploads\": %llu, \"failures\": %llu, \"frustum_tests\": %llu, \"frustum_culled\": %llu},\n",stronghold_finish?"true":"false",stronghold_state.actors,stronghold_state.walking,stronghold_state.visible,stronghold_state.peak_visible,stronghold_state.poses,stronghold_state.updates,stronghold_state.uploads,stronghold_state.failures,stronghold_state.frustum_tests,stronghold_state.frustum_culled);
            std::fprintf(f,"  \"source_flames\": {\"enabled\": %s, \"base_emitters\": %zu, \"renderer_capacity\": %u, \"studio_overlay_slots_reserved\": 16, \"architecture_dynamic_lights\": 0, \"actor_practical_limit\": 8, \"actor_practical_radius\": 12},\n",source_flames_enabled?"true":"false",flame_emitters.size(),WORLD_TORCH_FLAME_CAPACITY);
            std::fprintf(f,"  \"powder_snow\": {\"footprints\": %u, \"created\": %u, \"left_steps\": %u, \"right_steps\": %u, \"powder_particles\": %u, \"puffs_created\": %u, \"audio_steps\": %u, \"audio_ready\": %s, \"draws\": %llu, \"uploads\": %llu, \"failures\": %llu, \"footprint_capacity\": 384, \"powder_capacity\": 128, \"fade_seconds\": 180, \"imprint_updates\": %llu, \"depressed_texels\": %u, \"deepest_imprint\": %.5f},\n",snow_state.footprints,snow_state.created,snow_state.left,snow_state.right,snow_state.powder,snow_state.puffs,snow_state.audio_steps,snow_state.audio_ready?"true":"false",snow_state.draws,snow_state.uploads,snow_state.failures,snow_state.imprint_updates,snow_state.depressed_texels,snow_state.deepest_imprint);
            std::fprintf(f,"  \"purchased_particle_art\": {\"fire\": %s, \"smoke\": %s, \"fire_frames\": 144, \"fire_bytes\": %llu, \"smoke_bytes\": %llu, \"interpolated_frames\": true},\n",
                torch_state.purchased_sprites?"true":"false",particle_state.purchased_smoke?"true":"false",
                (unsigned long long)torch_state.sprite_bytes,(unsigned long long)particle_state.smoke_texture_bytes);
            std::fprintf(f,"  \"scene_particles\": {\"enabled\": true, \"startup_ready\": %s, \"authored_emitters\": 14, \"smoke_sources\": 8, \"dust_sources\": 6, \"width\": %d, \"height\": %d, \"bytes\": %llu, \"gpu_buffer_bytes\": %llu, \"allocations\": %llu, \"resolves\": %llu, \"draws\": %llu, \"instance_uploads\": %llu, \"failures\": %llu, \"peak_smoke\": %zu, \"peak_dust\": %zu, \"post_contact_pre_temporal\": true, \"weather_timing_includes_particles\": true},\n",
                particles_startup_ready?"true":"false",particle_state.width,particle_state.height,(unsigned long long)particle_state.target_bytes,
                (unsigned long long)particle_state.gpu_buffer_bytes,(unsigned long long)particle_state.allocations,(unsigned long long)particle_state.resolves,
                (unsigned long long)particle_state.draws,(unsigned long long)particle_state.instance_uploads,(unsigned long long)particle_state.failures,particle_peak_smoke,particle_peak_dust);
            std::fprintf(f,"  \"authored_weather\": {\"areas\": %zu, \"global_enabled\": %s, \"commits\": %u, \"frames\": %llu, \"failures\": %u, \"strikes\": %llu, \"thunder_delivered\": %llu, \"lightning_draws\": %llu, \"thunder_ready\": %s, \"listener_rain\": %.4f, \"listener_cloud\": %.4f, \"listener_fog\": %.4f, \"flash\": %.4f, \"strike_age\": %.4f},\n",authored_weather_areas.size(),authored_weather_global.enabled?"true":"false",authored_weather_commits,authored_weather_frames,authored_weather_failures,(unsigned long long)authored_storm_stats.strikes,(unsigned long long)authored_storm_stats.thunder_delivered,lightning_draws,weather_thunder_ready?"true":"false",authored_weather_at(cam.x,cam.z).rain,authored_weather_at(cam.x,cam.z).cloud,authored_weather_at(cam.x,cam.z).fog,authored_storm_output.flash,authored_storm_output.strike_age);
            std::fprintf(f,"  \"world_audio\": {\"enabled\": %s, \"authored_sounds\": %zu, \"prepared_sources\": %zu, \"decoded_assets\": %u, \"pcm_bytes\": %zu, \"active_voices\": %u, \"prepares\": %u, \"auto_loads\": %u, \"sound_only_applies\": %u, \"listener_updates\": %llu, \"update_failures\": %llu, \"gameplay_listener\": \"hero_head_with_view_orientation\"},\n",
                world_sound_active?"true":"false",studio.live.sounds.size(),world_sound_emitters.size(),world_sound_state.assets,world_sound_state.decoded_bytes,world_sound_state.active_voices,world_sound_prepares,world_sound_auto_loads,world_sound_fast_applies,world_sound_frames,world_sound_update_failures);
            std::fprintf(f,"  \"cutscenes\": {\"library_ready\":%s,\"progress_ready\":%s,\"starts\":%u,\"completions\":%u,\"skips\":%u,\"previews\":%u,\"audio_failures\":%u,\"active_id\":%llu,\"time\":%.9f,\"root_delta_max\":%.9f,\"aim_delta_max\":%.9f,\"lock_violations\":%u},\n",
                cutscene_library_ready?"true":"false",cutscene_progress_ready?"true":"false",cutscene_starts,cutscene_completions,cutscene_skips,cutscene_previews,cutscene_audio_failures,(unsigned long long)(cutscene_frame.active?cutscene_frame.id:0),cutscene_frame.time,cutscene_root_delta_max,cutscene_aim_delta_max,cutscene_lock_violations);
            std::fprintf(f,"  \"demo_audio\": {\"enabled\": %s, \"ready\": %s, \"driver\": %s, \"owns_device\": %s, \"loaded_beds\": %u, \"loaded_steps\": %u, \"voice_capacity\": %u, \"active_voices\": %u, \"pcm_bytes\": %zu, \"fire_sources\": %zu, \"updates\": %llu, \"update_failures\": %llu, \"steps\": %llu, \"walked_m\": %.9f, \"rejected_jumps\": %llu, \"zone_samples\": %llu, \"surface_steps\": [%llu,%llu,%llu,%llu], \"gains\": [",
                demo_audio_state.ready?"true":"false",demo_audio_state.ready?"true":"false",world_json::quote(demo_audio_state.driver).c_str(),
                demo_audio_state.owns_device?"true":"false",demo_audio_state.loaded_beds,demo_audio_state.loaded_steps,
                demo_audio_state.voice_capacity,demo_audio_state.active_voices,demo_audio_state.decoded_bytes,demo_audio_fire_sources,
                (unsigned long long)demo_audio_state.updates,(unsigned long long)demo_audio_update_failures,
                (unsigned long long)demo_audio_state.steps,demo_audio_state.walked_m,(unsigned long long)demo_audio_state.rejected_jumps,
                (unsigned long long)demo_audio_state.zone_samples,(unsigned long long)demo_audio_state.surface_steps[0],
                (unsigned long long)demo_audio_state.surface_steps[1],(unsigned long long)demo_audio_state.surface_steps[2],
                (unsigned long long)demo_audio_state.surface_steps[3]);
            for(unsigned i=0;i<WORLD_DEMO_AUDIO_BEDS;i++)std::fprintf(f,"%s%.7f",i?",":"",demo_audio_state.gains[i]);
            std::fprintf(f,"], \"targets\": [");
            for(unsigned i=0;i<WORLD_DEMO_AUDIO_BEDS;i++)std::fprintf(f,"%s%.7f",i?",":"",demo_audio_state.targets[i]);
            std::fprintf(f,"], \"distance_based_footsteps\": true, \"focus_muting\": true},\n");
            std::fprintf(f,"  \"tavern_light_return\": {\"enabled\": %s, \"irradiance\": [%.8f, %.8f, %.8f], \"steady_mean_sources\": true, \"exterior_roof_excluded\": true, \"composed_as_indirect\": true},\n",
                landscape.tavern_lighting_ready&&landscape.tavern_return_enabled?"true":"false",
                landscape.tavern_lighting.irradiance[0],landscape.tavern_lighting.irradiance[1],landscape.tavern_lighting.irradiance[2]);
            std::fprintf(f,"  \"shadow_map_side\": %d, \"shadow_depth_mib\": %.3f, \"soft_shadow_taps\": 9, \"world_stable_shadow_grid\": true,\n",shadow_side,double(shadow_side)*shadow_side*4/1048576.);
            std::fprintf(f,"  \"tree_shadow_culling\": {\"enabled\": %s, \"candidates\": %llu, \"rejected\": %llu, \"wind_guard_m\": 4, \"main_roots_unchanged\": true},\n",
                dressing.tree_shadow_culling?"true":"false",(unsigned long long)dressing.tree_shadow_candidates,(unsigned long long)dressing.tree_shadow_rejected);
            std::fprintf(f,"  \"grass_visibility\": {\"guard_m\": 4, \"full_source_wind_bounds\": true, \"opaque_wall_core_only\": true, \"procedural_frustum_culled\": %zu, \"procedural_wall_culled\": %zu, \"scanned_frustum_culled\": %zu, \"scanned_wall_culled\": %zu},\n",grass_frustum_culled,grass_wall_culled,scanned_frustum_culled,scanned_wall_culled);
            std::fprintf(f,"  \"grass_coverage\": {\"stable_alpha_rank\": true, \"depth_safe_temporal_gather\": true, \"root_fade_seconds\": 0.8, \"transition_root_cap\": %u, \"peak_transition_roots\": %zu, \"blend_uploads\": %zu, \"scanned_group_cap\": %zu, \"scanned_batch_capacity\": %zu, \"peak_scanned_group_candidates\": %zu, \"scanned_cap_drops\": %zu},\n",
                WORLD_COVER_GRASS_MAX_CAP*2,dressing.peak_blended_grass_roots,dressing.grass_blend_updates,
                Dressing::scanned_group_capacity,Dressing::scanned_batch_capacity,dressing.peak_scanned_group_candidates,dressing.scanned_grass_cap_drops);
            std::fprintf(f,"  \"local_atmosphere\": %s, \"village_atmosphere_weight\": %.6f,\n",
                (last_rain>0||last_fog>0)?"true":"false",world_atlas_village_weight(cam.x,cam.z));
            const auto final_wetness=world_weather_wetness::sample(active_weather_wetness,cam.x,cam.z,last_rain);
            if(weather_drip_cache){const auto&d=weather_drip_cache->stats;std::fprintf(f,"  \"weather_drip_sources\": {\"assets\": %u, \"unsupported\": %u, \"leaf_anchors\": %u, \"roof_anchors\": %u, \"refreshes\": %llu, \"triangles_tested\": %llu, \"alpha_rejected\": %llu, \"occluded\": %llu},\n",d.assets,d.unsupported,d.leaf_anchors,d.roof_anchors,(unsigned long long)d.refreshes,(unsigned long long)d.triangles_tested,(unsigned long long)d.alpha_rejected,(unsigned long long)d.occluded);}
            std::fprintf(f,"  \"weather_polish\": {\"wet_history_enabled\": %s, \"wet_history_layers\": %u, \"dampness\": %.6f, \"surface_film\": %.6f, \"history_evictions\": %llu, \"contact_queries\": %llu, \"drip_instances\": %u, \"authored_sky_draws\": %llu, \"sky_lightning_draws\": %llu, \"thunder_assets\": %u, \"thunder_bytes\": %zu, \"near_thunder\": %llu, \"distant_thunder\": %llu},\n",active_weather_wetness.enabled?"true":"false",active_weather_wetness.count,final_wetness.damp,final_wetness.film,(unsigned long long)authored_wetness_state.evictions,(unsigned long long)weather_state.contact_queries,weather_state.drip_instances,(unsigned long long)sky_state.authored_draws,(unsigned long long)sky_state.lightning_draws,demo_audio_state.thunder_assets,demo_audio_state.thunder_bytes,(unsigned long long)demo_audio_state.thunder_near_started,(unsigned long long)demo_audio_state.thunder_distant_started);
            std::fprintf(f,"  \"weather_cover_queries\": %llu, \"rain_splash_vertices_cap\": 38784,\n",(unsigned long long)weather_state.cover_queries);
            std::fprintf(f,R"JSON(  "rainforest_weather": %s, "rain_intensity": %.6f, "fog_strength": %.6f,
  "wind_x_mps": %.6f, "wind_z_mps": %.6f, "weather_mib": %.6f,
  "weather_width": %d, "weather_height": %d, "weather_half_width": %d, "weather_half_height": %d,
  "weather_resolves": %llu, "weather_resizes": %llu, "weather_failures": %zu,
  "weather_height_queries": %llu, "weather_cache_generations": %llu, "weather_terrain_cached": %s,
  "rain_instances": %u, "splash_instances": %u, "weather_fog_steps": 8, "weather_rows_per_update_cap": 8,
  "mean_weather_gpu_ms": %.6f, "pool_draw_frames": %zu, "pool_reflection_frames": %zu, "pool_surface_y": 134.0,
)JSON",world_atlas_rainforest_weight(cam.x,cam.z)>0&&(last_rain>0||last_fog>0)?"true":"false",last_rain,last_fog,last_wind_x,last_wind_z,weather_state.bytes/1048576.,
                weather_state.width,weather_state.height,weather_state.half_width,weather_state.half_height,
                (unsigned long long)weather_state.resolves,(unsigned long long)weather_state.resizes,weather_failures,
                (unsigned long long)weather_state.height_queries,(unsigned long long)weather_state.cache_generations,
                weather_state.terrain_cached?"true":"false",weather_state.rain_instances,weather_state.splash_instances,
                weather_gpu_sum/std::max(size_t(1),lighting_gpu_samples),pool_frames,pool_reflection_frames);
            std::fprintf(f,"  \"scenery_color_culling\": true, \"scenery_color_instances\": %zu, \"scenery_shadow_instances\": %zu, \"scenery_uploaded_instances\": %zu,\n",
                authored_state.color_instances,authored_state.shadow_instances,
                authored_state.near_trees+authored_state.mid_trees+authored_state.far_trees+authored_state.props);
            std::fprintf(f,"  \"combat_timing_windows\": {\"trial\": [%llu,%llu], \"crossing\": [%llu,%llu]},\n",(unsigned long long)combat_timing_windows[0][0],(unsigned long long)combat_timing_windows[0][1],(unsigned long long)combat_timing_windows[1][0],(unsigned long long)combat_timing_windows[1][1]);
            std::fprintf(f,"  \"recorded_dialogue\": {\"available_lines\": %u, \"required_lines\": 4, \"playbacks\": %u},\n",recorded_voice_lines,recorded_voice_playbacks);
            std::fprintf(f,"  \"combat_vfx\": {\"hits\": %u, \"parries\": %u, \"blocks\": %u, \"incoming\": %u, \"dust_bursts\": %u, \"trail_segments\": %u, \"draws\": %llu, \"peak_particles\": %u, \"peak_trails\": %u, \"failures\": %llu},\n",combat_vfx_state.hits,combat_vfx_state.parries,combat_vfx_state.blocks,combat_vfx_state.incoming,combat_vfx_state.dust_bursts,combat_vfx_state.trail_segments,combat_vfx_render_state.draws,combat_vfx_render_state.peak_particles,combat_vfx_render_state.peak_trails,combat_vfx_render_state.failures);
            std::fprintf(f,"  \"combat_motion\": {\"owned_pose_assets\": %u, \"actors\": %u, \"uploads\": %llu, \"textured_arms\": true},\n",combat_motion_state.pose_assets,combat_motion_state.actors,combat_motion_state.uploads);
            std::fprintf(f,"  \"arena_opponents\": {\"species\": \"%s\", \"orcs_ready\": %s, \"body_only\": %s, \"body_poses\": %u, \"arm_poses\": %u, \"measured_pivots\": %u, \"certified_weapon_pairs\": %u, \"actors\": %u, \"uploads\": %llu, \"original_textures\": %s, \"walk_stride_m\": %.3f, \"body_radius_m\": %.3f, \"body_height_m\": %.3f},\n",
                world_arena_opponents::name(arena_game.config().opponents),arena_orc_assets_ready?"true":"false",arena_orc_bank_state.body_only?"true":"false",
                arena_orc_bank_state.body_poses,arena_orc_bank_state.arm_poses,arena_orc_bank_state.measured_pivots,arena_orc_bank_state.certified_weapon_pairs,arena_orc_motion_state.actors,arena_orc_motion_state.uploads,arena_orc_original_textures_ready?"true":"false",arena_orc_bank_state.walk_stride_metres,arena_game.opponent_body_radius(),arena_game.opponent_body_height());
            std::fprintf(f,"  \"skeletal_hero_contact\": {\"queries\": %llu, \"samples\": %llu, \"failures\": %llu, \"phase_begin\": 0.26, \"phase_end\": 0.36, \"uses_current_world_transform\": true, \"retained_blade_history\": 0},\n",skeletal_contact_queries,skeletal_contact_samples,skeletal_contact_failures);
            std::fprintf(f,"  \"skeletal_hero_locomotion\": {\"foot_plants\": %llu, \"foot_releases\": %llu, \"foot_reach_releases\": %llu, \"maximum_foot_residual_m\": %.6f, \"maximum_foot_correction_m\": %.6f},\n",(unsigned long long)skeletal_hero_state.foot_plants,(unsigned long long)skeletal_hero_state.foot_releases,(unsigned long long)skeletal_hero_state.foot_reach_releases,skeletal_hero_state.maximum_foot_residual_m,skeletal_hero_state.maximum_foot_correction_m);
            std::fprintf(f,"  \"character_finish\": {\"facial_targets\":%u,\"expressive_frames\":%llu,\"blink_frames\":%llu,\"blink_peak\":%.6f,\"captured_starts\":%llu,\"captured_stops\":%llu,\"captured_turns\":%llu,\"captured_pivots\":%llu,\"captured_blocked\":%llu},\n",story_facial_targets,(unsigned long long)story_facial_frames,(unsigned long long)story_blink_frames,story_blink_peak,(unsigned long long)skeletal_hero_state.captured_starts,(unsigned long long)skeletal_hero_state.captured_stops,(unsigned long long)skeletal_hero_state.captured_turns,(unsigned long long)skeletal_hero_state.captured_pivots,(unsigned long long)skeletal_hero_state.captured_blocked);
            std::fprintf(f,"  \"combat_reaction_polish\": {\"physics\": {%s}, \"death\": {%s}},\n",
                combat_reaction_physics_fields(combat_reaction_physics_state).c_str(),combat_reaction_death_fields(combat_reaction_death_state).c_str());
            std::fprintf(f,"  \"combat_severing\": {\"anatomy_loaded\": %s, \"difficulty\": \"%s\", \"encounter_difficulty\": \"%s\", \"candidate_queries\": %llu, \"candidate_body_intersections\": %llu, \"candidate_steel_intersections\": %llu, \"geometry_misses\": %llu, \"limb_wounds\": %llu, \"limb_pulses\": %llu, \"active_wounds\": %u, \"peak_wounds\": %u, \"wound_capacity\": %u, \"last_enemy_severed_mask\": %u, \"last_enemy_weapon_hand\": %u},\n",
                combat_reaction_physics_state.loaded?"true":"false",world_combat_difficulty::name(preview_settings.difficulty),world_combat_difficulty::name(fight.difficulty),
                combat_anatomical_queries,combat_anatomical_body,combat_anatomical_steel,combat_anatomical_misses,
                (unsigned long long)sever_blood_state.wounds,(unsigned long long)sever_blood_state.pulses,
                sever_blood_state.active,sever_blood_state.peak,world_sever_blood::capacity,
                unsigned(fight.enemy_severed_parts),unsigned(fight.enemy_weapon_hand));
            std::fprintf(f,"  \"arena_player_sweep\": {\"contacts\": %llu, \"failures\": %llu, \"idle_depenetration\": false},\n",(unsigned long long)arena_player_sweep_contacts,(unsigned long long)arena_player_sweep_failures);
            std::fprintf(f,"  \"character_polish\": {\"enabled\": %s, \"pose_assets\": %u, \"reaction_assets\": %u, \"death_assets\": %u, \"keep_defeated\": %s, \"fallen_peak\": %u, \"death_starts\": %llu, \"directional_reactions\": %llu, \"fall_checked\": %s, \"grounded_checked\": %s, \"grounded_age\": %.6f},\n",combat_motion_state.polished?"true":"false",combat_motion_state.pose_assets,combat_motion_state.reaction_assets,combat_motion_state.death_assets,arena_game.keep_defeated?"true":"false",combat_motion_state.peak_fallen_instances,(unsigned long long)combat_motion_state.death_starts,(unsigned long long)combat_motion_state.directional_reactions,arena_qa_character_fall_checked?"true":"false",arena_qa_character_grounded_checked?"true":"false",arena_qa_character_grounded_age);
            {const auto &feedback=combat_feedback.stats();const auto &outcomes=feedback.outcomes;
             std::fprintf(f,"  \"combat_feedback_policy\": {\"misses\": %llu, \"body\": %llu, \"incoming\": %llu, \"blocks\": %llu, \"blocked_attacks\": %llu, \"parries\": %llu, \"audio_dispatches\": %llu, \"haptic_dispatches\": %llu, \"duplicate_events_suppressed\": %llu, \"paused_events_suppressed\": %llu, \"separate_attack_and_defense\": true},\n",
                (unsigned long long)outcomes[1],(unsigned long long)outcomes[2],(unsigned long long)outcomes[3],(unsigned long long)outcomes[4],(unsigned long long)outcomes[5],(unsigned long long)outcomes[6],
                (unsigned long long)combat_feedback_audio_dispatches,(unsigned long long)combat_feedback_haptic_dispatches,(unsigned long long)feedback.duplicates,(unsigned long long)feedback.paused_suppressed);}
            std::fprintf(f,"  \"combat_feedback\": {\"body_bursts\": %u, \"metal_bursts\": %u, \"body_audio\": %u, \"metal_audio\": %u, \"cut_windup_seconds\": %.3f, \"cut_cycle_seconds\": %.3f, \"parry_window_seconds\": %.3f},\n",combat_vfx_state.body_bursts,combat_vfx_state.metal_bursts,training_body_audio,training_metal_audio,WorldTrainingCombat::cut_windup_seconds,WorldTrainingCombat::cut_windup_seconds+WorldTrainingCombat::player_strike_seconds+WorldTrainingCombat::player_recovery_seconds,WorldTrainingCombat::parry_window_seconds);
            std::fprintf(f,"  \"volumetric_blood\": {\"enabled\": %s, \"provenance_loaded\": %s, \"variants\": %u, \"source_frames\": %u, \"source_vertices\": %llu, \"source_triangles\": %llu, \"geometry_bytes\": %llu, \"texture_bytes\": %llu, \"mesh_draws\": %llu, \"stain_draws\": %llu, \"frame_transitions\": %llu, \"failures\": %llu, \"spawned\": %llu, \"peak_meshes\": %u, \"peak_stains\": %u, \"counter_rebases\": %llu, \"resets\": %llu},\n",blood_enabled?"true":"false",blood_render_state.provenance_loaded?"true":"false",blood_render_state.loaded_variants,blood_render_state.source_frames,(unsigned long long)blood_render_state.source_vertices,(unsigned long long)blood_render_state.triangles,(unsigned long long)blood_render_state.geometry_bytes,(unsigned long long)blood_render_state.texture_bytes,(unsigned long long)blood_render_state.mesh_draws,(unsigned long long)blood_render_state.stain_draws,(unsigned long long)blood_render_state.frame_transitions,(unsigned long long)blood_render_state.failures,(unsigned long long)blood_state.mesh_spawned,blood_render_state.peak_meshes,blood_render_state.peak_stains,(unsigned long long)blood_state.counter_rebases,(unsigned long long)blood_state.resets);
            std::fprintf(f,"  \"combat_blood_intensity\": {\"fluid_scale\": %.2f, \"initial_particles\": %u, \"impacts\": %llu, \"secondary_sprays\": %llu, \"secondary_draws\": %llu, \"finisher_events\": %llu, \"finisher_draws\": %llu, \"initial_finisher_bursts\": %u, \"max_strength\": %.3f},\n",WorldBloodFrame{}.scale,world_combat_vfx::outgoing_body_particles,(unsigned long long)blood_state.impacts_spawned,(unsigned long long)blood_state.secondary_spawned,(unsigned long long)blood_render_state.secondary_draws,(unsigned long long)blood_state.finisher_events,(unsigned long long)blood_render_state.finisher_draws,combat_vfx_state.finisher_bursts,blood_state.max_strength);
            std::fprintf(f,"  \"ground_blood_persistence\": {\"enabled\": %s, \"capacity\": %u, \"witness_checked\": %s, \"witness_age\": %.6f, \"witness_serial\": %llu, \"stains_spawned\": %llu, \"stains_replaced\": %llu},\n",blood_effects.config().persistent_stains?"true":"false",world_blood_effects::stain_capacity,arena_qa_blood_persistent_checked?"true":"false",arena_qa_blood_persistent_age,(unsigned long long)arena_qa_blood_floor_serial,(unsigned long long)blood_state.stains_spawned,(unsigned long long)blood_state.stains_replaced);
            std::fprintf(f,"  \"combat_blood_motion\": {\"playback_rate\": %.2f, \"maximum_mesh_seconds\": %.6f, \"gravity\": %.2f, \"mist_lifetime\": %.2f, \"settled_checked\": %s, \"settle_elapsed\": %.6f, \"settle_stains\": %u},\n",world_blood_effects::playback_rate,blood_effects.config().mesh_duration+.035,world_combat_vfx::blood_gravity,world_combat_vfx::blood_mist_lifetime,arena_qa_blood_settled_checked?"true":"false",arena_qa_blood_settle_elapsed,arena_qa_blood_settle_stains);
            std::fprintf(f,"  \"player_usability\": {\"weapon_stows\": %u, \"weapon_draws\": %u, \"nameplate_frames\": %llu, \"labels\": %llu},\n",weapon_stows,weapon_draws,(unsigned long long)npc_nameplate_frames,(unsigned long long)npc_nameplate_labels);
            std::fprintf(f,"  \"combat_motion_polish\": {\"rigid_weapon_pairs\": %u, \"rigid_shader_draws\": %llu, \"body_draws_by_weapon\": [%llu,%llu,%llu,%llu], \"arms_draws_by_weapon\": [%llu,%llu,%llu,%llu], \"reaction_gait_interrupts\": %u, \"shoulder_frames\": %llu, \"shoulder_blocks\": %llu, \"shoulder_peak_m\": %.6f},\n",combat_motion_state.rigid_weapon_pairs,(unsigned long long)world_model_rigid_weapon_draws(),combat_motion_state.rigid_body_draws[0],combat_motion_state.rigid_body_draws[1],combat_motion_state.rigid_body_draws[2],combat_motion_state.rigid_body_draws[3],combat_motion_state.rigid_arms_draws[0],combat_motion_state.rigid_arms_draws[1],combat_motion_state.rigid_arms_draws[2],combat_motion_state.rigid_arms_draws[3],combat_motion_state.reaction_gait_interrupts,combat_shoulder_frames,combat_shoulder_blocks,combat_shoulder_peak);
            std::fprintf(f,"  \"combat_arm_grips\": {\"loaded\": %s, \"banks\": %u, \"certified_pairs\": %u, \"fp_certified_pairs\": %u, \"frames\": %llu, \"body_frames\": %llu, \"arms_frames\": %llu, \"disabled_frames\": %llu, \"failures\": %llu, \"body_draws\": %llu, \"arms_draws\": %llu, \"uploads\": %llu, \"metadata_bytes\": %llu, \"resident_cpu_bytes\": %llu, \"extra_gpu_bytes\": %llu, \"total_cpu_ms\": %.6f, \"maximum_cpu_ms\": %.6f},\n",grip_stats.loaded?"true":"false",grip_stats.banks,grip_stats.certified_pairs,grip_stats.fp_certified_pairs,grip_stats.frames,grip_stats.body_frames,grip_stats.arms_frames,grip_stats.disabled_frames,grip_stats.failures,grip_stats.body_draws,grip_stats.arms_draws,grip_stats.uploads,grip_stats.metadata_bytes,grip_stats.resident_cpu_bytes,grip_stats.extra_gpu_bytes,grip_stats.total_cpu_ms,grip_stats.maximum_cpu_ms);
            std::fprintf(f,"  \"combat_motion_continuity\": {\"guarded_entries\": %llu, \"late_combo_finishes\": %llu, \"recovery_settles\": %llu, \"reaction_settles\": %llu, \"moving_entries\": %llu, \"guard_recoils\": %llu, \"reaction_counters\": %llu, \"shield_hit_returns\": %llu},\n",continuity_stats.guarded_entries,continuity_stats.late_combo_finishes,continuity_stats.recovery_settles,continuity_stats.reaction_settles,continuity_stats.moving_entries,continuity_stats.guard_recoils,continuity_stats.reaction_counters,continuity_stats.shield_hit_returns);
            const auto &focus_stats=combat_focus_camera.stats();
            std::fprintf(f,"  \"combat_focus_camera\": {\"transition_seconds\": %.6f, \"transitions\": %llu, \"transition_frames\": %llu, \"resets\": %llu, \"visibility_clamps\": %llu, \"maximum_transition_error_rad\": %.9f, \"maximum_observed_completion_s\": %.9f, \"maximum_transition_frame_s\": %.9f, \"render_orientation_only\": true},\n",WorldCombatFocusCamera::duration_seconds,(unsigned long long)focus_stats.transitions,(unsigned long long)focus_stats.transition_frames,(unsigned long long)focus_stats.resets,(unsigned long long)focus_stats.visibility_clamps,focus_stats.maximum_transition_error,focus_stats.maximum_completion_seconds,focus_stats.maximum_transition_frame_seconds);
            std::fprintf(f,"  \"camera_view\": {\"third_person\": %s, \"toggles\": %u},\n",third_person?"true":"false",camera_toggles);
            std::fprintf(f,"  \"story_hero\": {\"requested\": %s, \"civilian_ready\": %s, \"armored\": %s, \"appearance\": %s, \"changes\": %llu, \"failures\": %llu, \"loading_hold_frames\": %llu, \"plain_color_frames\": %llu, \"plain_indoor_frames\": %llu, \"plain_preloan_frames\": %llu, \"default_third_person\": true, \"armor_requires_equipped_mail\": true},\n",story_civilian_requested?"true":"false",story_civilian_ready?"true":"false",story_appearance_armored?"true":"false",world_json::quote(story_appearance_loaded).c_str(),(unsigned long long)story_appearance_changes,(unsigned long long)story_appearance_failures,(unsigned long long)story_appearance_loading_frames,(unsigned long long)story_plain_color_frames,(unsigned long long)story_plain_indoor_frames,(unsigned long long)story_plain_preloan_frames);
            std::fprintf(f,"  \"story_hero_special\": {\"swimming_color_frames\": %llu, \"swimming_first_person_frames\": %llu, \"death_color_frames\": %llu, \"swimming_capture_available\": %s, \"death_capture_available\": %s, \"water_draw_before_resolve\": true, \"legacy_story_player_disabled\": true},\n",(unsigned long long)story_swim_color_frames,(unsigned long long)story_swim_first_person_frames,(unsigned long long)story_dead_color_frames,skeletal_story_swim_available?"true":"false",skeletal_story_death_available?"true":"false");
            std::fprintf(f,"  \"rainforest_weight\": %.6f,\n",world_atlas_rainforest_weight(cam.x,cam.z));
            std::fprintf(f,"  \"zero_coverage_culling\": {\"color_enabled\": %s, \"shadow_enabled\": false, \"guard_pixels\": 2.99, \"existing_zero_coverage_endpoint_pixels\": 3},\n",zero_coverage_cull_enabled?"true":"false");
            std::fprintf(f,"  \"zero_contribution_work_skip\": {\"terrain_noise_existing_endpoints\": true, \"primary_probe_exact_zero\": true, \"opaque_zero_sun_nine_tap_only\": true, \"untagged_leaf_transmission_unchanged\": true},\n");
            std::fprintf(f,"  \"shadow_fetch_grouping\": {\"supported\": %s, \"enabled\": %s, \"logical_taps\": 9, \"scalar_border_fallback\": true, \"four_tap_unchanged\": true},\n",
                shadow_gather_supported?"true":"false",shadow_gather_enabled?"true":"false");
            std::fprintf(f,"  \"vegetation_selection_work\": {\"grass_classification_refreshes\": %llu, \"grass_classified_seeds\": %llu, \"grass_structural_revision\": %llu, \"tree_dispatches\": %llu, \"grass_dispatches\": %llu, \"tree_fallbacks\": %llu, \"grass_fallbacks\": %llu, \"ranges_caps_order_preserved\": true},\n",
                (unsigned long long)dressing.grass.classification_refreshes,(unsigned long long)dressing.grass.classified_seeds,
                (unsigned long long)dressing.grass.classification_revision,(unsigned long long)dressing.tree_dispatches,
                (unsigned long long)dressing.grass_dispatches,(unsigned long long)dressing.tree_fallbacks,
                (unsigned long long)dressing.grass_fallbacks);
            std::fprintf(f,"  \"wildlife_material_preparation\": {\"enabled\": %s, \"calls\": %llu, \"main_passes\": %llu, \"reflection_passes\": %llu, \"shadow_passes\": %llu, \"pose_draws\": %llu, \"failures\": %llu, \"loading_only\": true, \"main_color_attachments\": 2, \"reflection_color_attachments\": 1, \"walking_fences\": 0},\n",
                wildlife_preparation_enabled?"true":"false",(unsigned long long)wildlife_preparation.calls,
                (unsigned long long)wildlife_preparation.main_passes,(unsigned long long)wildlife_preparation.reflection_passes,
                (unsigned long long)wildlife_preparation.shadow_passes,(unsigned long long)wildlife_preparation.pose_draws,
                (unsigned long long)wildlife_preparation.failures);
            std::fprintf(f,"  \"water_reflection_cache\": {\"captured_frames\":%llu,\"reused_frames\":%llu,\"projection_paired_with_image\":true,\"stationary_reuse_only\":true},\n",(unsigned long long)water_mirror_captured_frames,(unsigned long long)water_mirror_reused_frames);
            std::fprintf(f,"  \"river_reflection_scissor\": {\"enabled\":%s,\"sole_river_frames\":%llu,\"restricted_frames\":%llu,\"geometry_pixels\":%llu,\"full_pixels\":%llu,\"full_sky\":true,\"same_resolution\":true,\"distortion_uv_guard\":0.009,\"blur_texel_guard\":8},\n",river_reflection_scissor_enabled?"true":"false",(unsigned long long)river_reflection_support_frames,(unsigned long long)river_reflection_restricted_frames,(unsigned long long)river_reflection_geometry_pixels,(unsigned long long)river_reflection_full_pixels);
            std::fprintf(f,"  \"pool_reflection_scissor\": {\"enabled\": %s, \"restricted_frames\": %zu, \"geometry_pixels\": %llu, \"full_pixels\": %llu, \"distortion_uv_bound\": 0.009, \"blur_texel_bound\": 5, \"full_resolution\": true, \"full_sky\": true},\n",
                pool_reflection_scissor_enabled?"true":"false",pool_reflection_scissor_frames,
                (unsigned long long)pool_reflection_geometry_pixels,(unsigned long long)pool_reflection_full_pixels);
            std::fprintf(f,"  \"pool_reflection_clip\": {\"enabled\": %s, \"tests\": %llu, \"proven_outside\": %llu, \"skipped_passes\": %llu, \"pool_owner_only\": true, \"opaque_snapshot_unchanged\": true, \"loading_warm_unchanged\": true, \"default_clip_planes\": true},\n",
                pool_reflection_clip_enabled?"true":"false",(unsigned long long)pool_reflection_clip_tests,
                (unsigned long long)pool_reflection_clip_outside,(unsigned long long)pool_reflection_clip_skips);
            std::fprintf(f,"  \"terrain_shadow_culling\": {\"exact_clip_planes\": true, \"guard_m\": 4, \"rejected_tiles\": %zu, \"submitted_tiles\": %zu},\n",
                terrain.shadow_clip_rejections,terrain.shadow_tile_draws);
            std::fprintf(f,"  \"horizon_selection_cache\": {\"selections\": %llu, \"reused_upload_batches\": %llu, \"uploads\": %llu, \"uploaded_instances\": %llu, \"same_selected_roots\": true, \"publication_safe\": true},\n",
                (unsigned long long)horizon.selections,(unsigned long long)horizon.reused_upload_batches,
                (unsigned long long)horizon.uploads,(unsigned long long)horizon.uploaded_instances);
            std::fprintf(f,"  \"horizon_spatial_grid\": {\"cell_size_m\": 512, \"classifications\": %llu, \"cells\": %llu, \"retained_cells\": %llu, \"candidate_roots\": %llu, \"fallbacks\": %llu, \"exact_point_predicate\": true, \"source_order_preserved\": true},\n",
                (unsigned long long)horizon.grid_classifications,(unsigned long long)horizon.grid_cells,
                (unsigned long long)horizon.grid_retained_cells,(unsigned long long)horizon.grid_candidate_roots,
                (unsigned long long)horizon.grid_fallbacks);
            std::fprintf(f,"  \"lighting\": {\"hour\": %.6f, \"cloud\": %.6f, \"sun_bearing_degrees\": %.6f, \"sun_strength\": %.6f, \"sky_fill\": %.6f, \"exposure\": %.6f, \"visual_time\": %.6f, \"display_transfer\": \"sRGB\", \"opaque_environment_specular\": true},\n",
                hour,cloud,bearing,sun_strength,sky_fill,exposure,final_visual_time);
            const float biome_weight=world_atlas_rainforest_weight(cam.x,cam.z);
            const auto biome=world_rainforest_atmosphere(cloud,biome_weight,visual_surface_finish);
            const auto biome_light=local_lighting(hour,cloud,final_visual_time,biome_weight,bearing,sun_strength,sky_fill);
            std::fprintf(f,"  \"rainforest_atmosphere\": {\"weight\": %.6f, \"blend\": %.6f, \"cloud\": %.6f, \"sun_multiplier\": %.6f, \"sky_multiplier\": %.6f, \"haze\": %.8f, \"mist_weight\": %.6f, \"fog_color\": [%.6f,%.6f,%.6f], \"sky_zenith\": [%.6f,%.6f,%.6f], \"smooth_biome_transition\": true, \"quality_settings_respected\": true},\n",
                biome_weight,biome.blend,biome.cloud,biome.sun_multiplier,biome.sky_multiplier,biome_light.haze,biome.mist_weight,
                biome_light.fog.x,biome_light.fog.y,biome_light.fog.z,biome_light.skyZenith.x,biome_light.skyZenith.y,biome_light.skyZenith.z);
            std::fprintf(f,"  \"village_color_grade\": %s, \"village_grade_strength\": %.6f, \"village_grade_gpu_bytes\": 32768,\n",village_grade_ready?"true":"false",grade_strength);
            std::fprintf(f,"  \"village_farms\": {\"plots\": %zu, \"corn_clusters\": 60, \"wheat_clusters\": 60, \"corn_plants\": 1200, \"wheat_stems\": 21120, \"cultivated_area_m2\": 2376, \"inside_town_outside_castle\": true, \"rooted_crop_wind\": true, \"tilled_soil\": true, \"far_fade_m\": [280,320]},\n",size_t(WORLD_VILLAGE_FARM_COUNT));
            std::fprintf(f,"  \"local_light_shadows\": {\"supported\": %s, \"cube_side\": %d, \"texture_mib\": %.3f, \"faces_rendered\": %llu, \"completed_cycles\": %llu, \"source_selections\": %llu, \"first_refreshes\": %llu, \"failures\": %llu, \"update_cpu_ms_total\": %.3f, \"max_shadowed_lights\": 1, \"steady_faces_per_frame\": 2, \"double_buffered\": true},\n",
                local_light_shadows_supported?"true":"false",local_shadow_side,local_shadow_bytes/1048576.,(unsigned long long)local_light_shadow.faces,(unsigned long long)local_light_shadow.cycles,(unsigned long long)local_light_shadow.selections,(unsigned long long)local_light_shadow.first_refreshes,(unsigned long long)local_light_shadow.failures,local_light_shadow.cpu_ms);
            std::fprintf(f,"  \"village_torches\": {\"models\": %zu, \"emitters\": %u, \"light_cap\": %u, \"peak_lights\": %zu, \"peak_visible_flames\": %zu, \"flame_draws\": %llu, \"main_frames\": %zu, \"reflection_frames\": %zu, \"buffer_bytes\": %zu, \"failures\": %zu, \"shared_smooth_flicker\": true, \"wall_plane_and_hall_clip\": true, \"point_shadow_maps\": %s},\n",
                std::size(village_torch_emitters),unsigned(std::size(village_torch_emitters)),WorldTorchLights::capacity,torch_peak_lights,torch_peak_visible,
                (unsigned long long)torch_state.draws,torch_main_frames,torch_reflection_frames,torch_state.gpu_buffer_bytes,torch_failures,local_light_shadow.faces?"true":"false");
            std::fprintf(f,"  \"village_watch\": {\"actors\": %zu, \"visible\": %zu, \"peak_visible\": %zu, \"pose_assets\": %zu, \"blend_batches\": %zu, \"gpu_pose_blending\": %s, \"updates\": %llu, \"rooted_stations\": true},\n",
                watch_state.actors,watch_state.visible,watch_peak_visible,watch_state.loaded_pose_assets,world_village_watch_pose_count,
                watch_state.loaded_pose_assets==world_village_watch_pose_count?"true":"false",(unsigned long long)watch_state.updates);
            std::fprintf(f,"  \"castle_dragon_stages\": %u,\n",dragon_stage_mask);
            std::fprintf(f,"  \"castle_dragon_actors\": [");
            for(unsigned i=0;i<2;i++){
                const auto &a=dragon_final[i];
                std::fprintf(f,"%s{\"variant\": %u, \"stage\": %u, \"position\": [%.6f,%.6f,%.6f], \"mouth\": [%.6f,%.6f,%.6f], \"direction\": [%.6f,%.6f,%.6f], \"breath\": %.6f, \"cycle_time\": %.6f}",
                    i?",":"",i,unsigned(a.stage),a.x,a.y,a.z,a.mouth[0],a.mouth[1],a.mouth[2],a.direction[0],a.direction[1],a.direction[2],a.breath,a.cycle_time);
            }
            std::fprintf(f,"],\n");
            std::fprintf(f,"  \"mean_dragon_breath_gpu_ms\": %.6f,\n",lighting_gpu_samples?dragon_gpu_sum/lighting_gpu_samples:0.);
            std::fprintf(f,"  \"castle_dragons\": {\"enabled\": %s, \"active\": %s, \"actors\": %u, \"pose_assets\": %u, \"blend_batches\": %u, \"peak_visible\": %u, \"peak_breathing\": %u, \"loads\": %llu, \"unloads\": %llu, \"failures\": %llu, \"updates\": %llu, \"uploads\": %llu, \"cycle_seconds\": %.6f, \"time_offset\": %.3f, \"breath_frames\": %llu, \"peak_particles\": %u, \"breath_failures\": %llu, \"breath_buffer_bytes\": %zu, \"breath_sprite_bytes\": %llu},\n",
                dragons_enabled?"true":"false",dragon_state.active?"true":"false",dragon_state.actors,dragon_state.pose_assets,dragon_state.blend_batches,dragon_peak_visible,dragon_peak_breathing,
                (unsigned long long)dragon_state.loads,(unsigned long long)dragon_state.unloads,(unsigned long long)dragon_state.failures,(unsigned long long)dragon_state.updates,(unsigned long long)dragon_state.uploads,
                world_castle_dragons_cycle_seconds(),dragon_time_offset,(unsigned long long)dragon_breath_frames,dragon_peak_particles,(unsigned long long)dragon_breath_failures,
                dragon_breath_accumulated.gpu_buffer_bytes,(unsigned long long)dragon_breath_accumulated.sprite_bytes);
            std::fprintf(f,"  \"training_combat\": {\"qa\": %s, \"events\": %u, \"state\": %u, \"round\": %u, \"health\": %.3f, \"stamina\": %.3f, \"hits\": %u, \"parries\": %u, \"blocks\": %u, \"rejected_attacks\": %u, \"directions\": %u, \"starts\": %u, \"paused_frames\": %u, \"begin_frame\": %llu, \"end_frame\": %llu, \"relative_mouse\": %s, \"capture_ms\": %.3f, \"capture_focused\": %s, \"audio_ready\": %s, \"audio_events\": %u},\n",qa_training?"true":"false",qa_training_events,unsigned(fight.state),fight.round,fight.health,fight.stamina,fight.hits,fight.parries,fight.blocks,fight.rejected_attacks,fight.direction_mask,fight_starts,fight_paused_frames,(unsigned long long)fight_begin_frame,(unsigned long long)fight_end_frame,SDL_GetRelativeMouseMode()?"true":"false",fight_capture_max_ms,fight_capture_focused?"true":"false",training_audio_ready?"true":"false",training_audio_events);
            std::fprintf(f,"  \"castle_training\": {\"actors\": %zu, \"trainees\": %zu, \"instructors\": %zu, \"visible\": %zu, \"peak_visible\": %zu, \"pose_assets\": %zu, \"blend_batches\": %zu, \"gpu_pose_blending\": %s, \"updates\": %llu},\n",
                training_state.actors,training_state.trainees,training_state.instructors,training_state.visible,training_peak_visible,
                training_state.loaded_pose_assets,world_castle_training_blend_count,training_state.loaded_pose_assets==world_castle_training_pose_count?"true":"false",(unsigned long long)training_state.updates);
            std::fprintf(f,"  \"castle_creek\": {\"legacy_surface_enabled\": %s, \"source_scene_owns_water\": %s, \"draw_frames\": %llu, \"reflection_frames\": %llu, \"water_surface_y\": 22.9, \"bridge_deck_y\": 24.02, \"outside_outer_town_walls\": true, \"polyline_points\": %zu, \"explicit_surface_mask\": true},\n",city_showcase?"false":"true",city_showcase?"true":"false",(unsigned long long)creek_frames,(unsigned long long)creek_reflection_frames,std::size(world_castle_creek_points));
            std::fprintf(f,"  \"road_traffic\": {\"loaded\": %s, \"validated\": %s, \"actors\": %u, \"walkers\": %u, \"riders\": %u, \"pullcarts\": %u, \"moving\": %u, \"yielding\": %u, \"barrier_waiting\": %u, \"visible\": %u, \"pose_assets\": %u, \"draw_groups\": %u, \"updates\": %llu, \"simulation_steps\": %llu, \"uploads\": %llu, \"draws\": %llu, \"failures\": %llu, \"blocked\": %llu, \"travelled_m\": %.6f},\n",
                road_traffic_loaded?"true":"false",road_traffic_validated?"true":"false",road_traffic_state.actors,road_traffic_state.walkers,road_traffic_state.riders,road_traffic_state.pullcarts,road_traffic_state.moving,road_traffic_state.yielding,road_traffic_state.barrier_waiting,road_traffic_state.visible,road_traffic_state.pose_assets,road_traffic_state.draw_groups,(unsigned long long)road_traffic_state.updates,(unsigned long long)road_traffic_state.simulation_steps,(unsigned long long)road_traffic_state.uploads,(unsigned long long)road_traffic_state.draws,(unsigned long long)road_traffic_state.failures,(unsigned long long)road_traffic_state.blocked,road_traffic_state.travelled);
            std::fprintf(f,"  \"road_rigid_batches\": {\"uploads\": %llu, \"unchanged_uploads_skipped\": %llu},\n",(unsigned long long)road_traffic_state.rigid_uploads,(unsigned long long)road_traffic_state.rigid_uploads_skipped);
            std::fprintf(f,"  \"village_population\": {\"residents\": %zu, \"knights\": %zu, \"archers\": %zu, \"traders\": %zu, \"travelers\": %zu, \"walking\": %zu, \"idling\": %zu, \"visible\": %zu, \"peak_visible\": %zu, \"pose_assets\": %zu, \"draw_groups\": %zu, \"uploads\": %llu, \"updates\": %llu},\n",
                population_state.residents,population_state.knights,population_state.archers,population_state.traders,population_state.travelers,
                population_state.walking,population_state.idling,population_state.visible,population_peak_visible,population_state.loaded_pose_assets,
                population_state.draw_groups,(unsigned long long)population_state.uploads,(unsigned long long)population_state.updates);
            std::fprintf(f,"  \"castle_streets\": {\"sheltered_residents\": %zu, \"tavern_residents\": %zu, \"street_residents\": %zu, \"seated\": %zu, \"practical_emitters\": %zu, \"global_flame_capacity\": %u},\n",
                population_state.sheltered_residents,population_state.tavern_residents,population_state.street_residents,population_state.seated,
                std::size(castle_street_torch_emitters),unsigned(WORLD_TORCH_FLAME_CAPACITY));
            std::fprintf(f,"  \"early_opaque\": {\"alias\": \"village_tavern\", \"enabled\": %s, \"frames\": %zu, \"late_skipped_frames\": %zu, \"same_color_program\": true, \"shadow_reflection_unchanged\": true},\n",
                early_opaque_enabled?"true":"false",early_opaque_frames,late_opaque_skipped_frames);
            std::fprintf(f,"  \"creek_water_preparation\": {\"startup_ready\": %s, \"startup_ms\": %.3f, \"startup_render_passes\": %u, \"first_reflection_frame\": %llu, \"reflection_play_allocations\": %llu, \"sky_reflection_play_allocations\": %llu, \"water_scene_play_allocations\": %llu},\n",
                creek_water_prewarmed?"true":"false",creek_water_prewarm_ms,creek_water_startup_passes,(unsigned long long)first_reflection_frame,
                (unsigned long long)reflection_play_allocations,(unsigned long long)sky_reflection_play_allocations,(unsigned long long)water_scene_play_allocations);
            std::fprintf(f,"  \"population_frustum\": {\"enabled\": %s, \"draw_calls\": %llu, \"culled_batches\": %llu, \"submitted_batches\": %llu, \"shadow_casters_unchanged\": true},\n",
                population_frustum_enabled?"true":"false",(unsigned long long)population_state.frustum_draw_calls,
                (unsigned long long)population_state.frustum_culled_batches,(unsigned long long)population_state.frustum_submitted_batches);
            std::fprintf(f,"  \"village_animation\": {\"gpu_pose_blending\": %s, \"pose_batch_capacity\": %zu, \"pose_contract_version\": %u},\n",
                population_state.loaded_pose_assets==world_village_pose_group_count?"true":"false",world_village_pose_blend_group_count,
                unsigned(WORLD_MODEL_POSE_BLEND_VERSION));
            std::fprintf(f,"  \"village_pets\": {\"pets\": %zu, \"cats\": %zu, \"dogs\": %zu, \"walking\": %zu, \"idling\": %zu, \"visible\": %zu, \"peak_visible\": %zu, \"pose_assets\": %zu, \"pose_batch_capacity\": %zu, \"gpu_pose_blending\": %s, \"draw_groups\": %zu, \"uploads\": %llu, \"updates\": %llu},\n",
                pets_state.pets,pets_state.cats,pets_state.dogs,pets_state.walking,pets_state.idling,pets_state.visible,pets_peak_visible,
                pets_state.loaded_pose_assets,world_village_pet_blend_count,pets_state.loaded_pose_assets==world_village_pet_pose_count?"true":"false",
                pets_state.draw_groups,(unsigned long long)pets_state.uploads,(unsigned long long)pets_state.updates);
            std::fprintf(f,"  \"wildlife\": {\"habitat\": %d, \"actors\": %zu, \"walking\": %zu, \"idling\": %zu, \"visible\": %zu, \"peak_visible\": %zu, \"species\": %zu, \"pose_assets\": %zu, \"peak_pose_assets\": %zu, \"loads\": %llu, \"unloads\": %llu, \"load_failures\": %llu, \"unsafe_roots\": %llu, \"updates\": %llu, \"uploads\": %llu, \"habitat_residency\": true, \"scene_cache_entries\": %zu, \"scene_cache_references\": %zu, \"scene_cache_source_bytes\": %zu},\n",
                wildlife_state.habitat,wildlife_state.actors,wildlife_state.walking,wildlife_state.idling,wildlife_state.visible,wildlife_peak_visible,
                wildlife_state.species,wildlife_state.pose_assets,wildlife_state.peak_pose_assets,(unsigned long long)wildlife_state.loads,
                (unsigned long long)wildlife_state.unloads,(unsigned long long)wildlife_state.load_failures,(unsigned long long)wildlife_state.unsafe_roots,
                (unsigned long long)wildlife_state.updates,(unsigned long long)wildlife_state.uploads,wildlife_scene_cache.entries,wildlife_scene_cache.references,wildlife_scene_cache.source_bytes);
            std::fprintf(f,"  \"jungle_expansion\": {\"enabled\": %s, \"flying_birds\": %zu, \"area_m2\": 98700},\n",access("jungle_expansion.json",F_OK)==0?"true":"false",wildlife_state.flying);
            std::fprintf(f,"  \"village_mask_side\": 513, \"village_mask_gpu_mib\": %.6f,\n",513.*513.*4./1048576.);
            std::fprintf(f,"  \"bandit_camp\": {\"active\": %s, \"actors\": %zu, \"walking\": %zu, \"running\": %zu, \"sentries\": %zu, \"visible\": %zu, \"reflection_visible\": %zu, \"shadow_visible\": %zu, \"peak_visible\": %zu, \"pose_assets\": %zu, \"draw_groups\": %zu, \"peak_pose_assets\": %zu, \"loads\": %llu, \"unloads\": %llu, \"load_failures\": %llu, \"unsafe_roots\": %llu, \"updates\": %llu, \"uploads\": %llu, \"lights_off\": %s, \"light_count\": %zu, \"peak_lights\": %zu, \"flame_draws\": %llu, \"peak_visible_flames\": %zu, \"emitters\": %zu, \"mask_cpu_side\": 257, \"mask_gpu_mib\": %.6f},\n",
                bandit_state.active?"true":"false",bandit_state.actors,bandit_state.walking,bandit_state.running,bandit_state.sentries,
                bandit_state.visible,bandit_state.reflection_visible,bandit_state.shadow_visible,bandit_peak_visible,
                bandit_state.pose_assets,bandit_state.draw_groups,bandit_state.peak_pose_assets,
                (unsigned long long)bandit_state.loads,(unsigned long long)bandit_state.unloads,(unsigned long long)bandit_state.load_failures,
                (unsigned long long)bandit_state.unsafe_roots,(unsigned long long)bandit_state.updates,(unsigned long long)bandit_state.uploads,
                qa_bandit_lights_off?"true":"false",bandit_light_count,bandit_peak_lights,(unsigned long long)bandit_flame_draws,
                bandit_peak_flames,std::size(bandit_torch_emitters),513.*513.*4./1048576.);
            std::fprintf(f,"  \"forest_shelter\": {\"enabled\": %s, \"trees\": %u, \"side\": 513, \"gpu_mib\": %.6f, \"minimum_access\": %.6f, \"mean_access\": %.6f, \"arrival_region\": %s, \"height_min\": %.6f, \"height_range\": %.6f, \"startup_only\": true, \"extra_samplers\": 0, \"extra_shadow_passes\": 0},\n",
                landscape.forest_shelter_enabled?"true":"false",landscape.forest_shelter.trees,513.*513.*4./1048576.,
                landscape.forest_shelter.minimum_access,landscape.forest_shelter.mean_access,landscape.forest_shelter.arrival_region?"true":"false",landscape.forest_shelter.height_min,landscape.forest_shelter.height_range);
            std::fprintf(f,"  \"village_surfaces\": {\"directional_sky\": %s, \"directional_side\": %d, \"height_slices\": %d, \"directional_mib\": %.6f, \"ground_history\": %s, \"ground_history_side\": %d, \"ground_history_mib\": %.6f, \"terrain_sampler_count\": 16, \"stochastic_paving\": %s},\n",
                village_directional_active?"true":"false",village_directional_side,village_directional_slices,village_directional_bytes/1048576.,
                village_history_active?"true":"false",world_village_ground_side,village_history_bytes/1048576.,village_history_active?"true":"false");
            std::fprintf(f,"  \"castle_practicals\": {\"enabled\": %s, \"count\": 4, \"radius_m\": 20, \"emitter_y_m\": 26.895, \"masked_to_receiving_hall\": true, \"shadow_maps\": false},\n",castle_practicals_active?"true":"false");
            std::fprintf(f,"  \"texture_cache_entries_at_load\": %zu, \"texture_cache_source_mib\": %.6f, \"texture_cache_source_budget_mib\": %.6f, \"texture_cache_unmanaged_uploads\": %zu,\n",
                loaded_texture_state.entries,loaded_texture_state.source_bytes/1048576.,TEXTURE_CACHE_SOURCE_BUDGET_BYTES/1048576.,loaded_texture_state.unmanaged_uploads);
            std::fprintf(f,"  \"texture_source_storage\": {\"file_bytes\": %zu, \"heap_bytes\": %zu, \"file_entries\": %zu, \"loading_trims\": %zu, \"trim_failures\": %zu, \"read_only_exact_keys\": true, \"fixed_budget\": true, \"gpu_images_unchanged\": true},\n",
                wildlife_scene_cache.source_file_bytes,wildlife_scene_cache.source_heap_bytes,wildlife_scene_cache.source_file_entries,
                wildlife_scene_cache.source_page_trims,wildlife_scene_cache.source_page_trim_failures);
            const auto depth_materials=world_model_depth_material_stats();
            std::fprintf(f,"  \"watch_station_safety\": {\"checked\": %zu, \"required\": %zu, \"capsule_radius_m\": 0.70, \"height_m\": 1.85},\n",
                watch_safe_stations,watch_state.actors);
            std::fprintf(f,"  \"depth_materials\": {\"opaque_neutral_draws\": %llu, \"original_masked_draws\": %llu, \"images_unchanged\": true},\n",
                (unsigned long long)depth_materials.opaque_draws,(unsigned long long)depth_materials.masked_draws);
            std::fprintf(f,"  \"texture_blocks\": {\"enabled\": %s, \"candidates\": %llu, \"cached_textures\": %llu, \"reference_rgba_bytes\": %llu, \"block_bytes\": %llu, \"cache_misses\": %llu, \"rejected_cache\": %llu, \"upload_failures\": %llu, \"offline_encoder\": true, \"full_resolution\": true, \"counter_scope\": \"cumulative_immediate_upload_attempts\", \"all_mip_levels_preserved\": true, \"linear_data_requires_certificate\": true, \"masked_color_maps_original_rgba\": true, \"facade_auxiliary_linear_maps_original_rgba\": true, \"runtime_encoding\": false},\n",
                texture_block_state.enabled?"true":"false",(unsigned long long)texture_block_state.candidates,
                (unsigned long long)texture_block_state.cached_textures,(unsigned long long)texture_block_state.reference_rgba_bytes,
                (unsigned long long)texture_block_state.block_bytes,(unsigned long long)texture_block_state.cache_misses,
                (unsigned long long)texture_block_state.rejected_cache,(unsigned long long)texture_block_state.upload_failures);
            std::fprintf(f,"  \"mean_terrain_gpu_ms\": %.6f, \"mean_water_gpu_ms\": %.6f, \"mean_temporal_gpu_ms\": %.6f, \"mean_post_gpu_ms\": %.6f,\n",
                terrain_gpu_sum/std::max(size_t(1),lighting_gpu_samples),water_gpu_sum/std::max(size_t(1),lighting_gpu_samples),
                temporal_gpu_sum/std::max(size_t(1),lighting_gpu_samples),post_gpu_sum/std::max(size_t(1),lighting_gpu_samples));
            std::fprintf(f,R"JSON(  "scenic": %s, "view": %s, "benchmark": %s,
  "camera_final": {"x": %.6f, "y": %.6f, "z": %.6f, "yaw": %.6f, "pitch": %.6f, "fly": %s},
  "swimming": %s, "walking_capsule_radius_m": 0.37, "manual_collision_steps": %zu, "manual_slope_steps": %zu, "manual_sliding_steps": %zu,
  "starting_mask_side": 513, "starting_mask_gpu_mib": %.6f,
  "authored_source_files": %zu, "authored_unique_aliases": %zu, "authored_colliders": %zu,
  "benchmark_route": {"active": %s, "id": %s, "length_m": %.6f,
    "speed_mps": %.3f, "step_seconds": %.9f, "walking_frames": %zu, "movement_commands": %zu,
    "elapsed_sim_seconds": %.6f, "attempted_distance_m": %.6f, "actual_distance_m": %.6f,
    "waypoints_reached": %zu, "collision_blocked_steps": %zu, "slope_blocked_steps": %zu,
    "sliding_steps": %zu, "waiting_frames": %zu, "complete": %s, "return_error_m": %.6f},
  "authored_sources": [
)JSON",world_json::quote(launch->scenic).c_str(),world_json::quote(launch->view).c_str(),launch->benchmark?"true":"false",
                cam.x,cam.y,cam.z,cam.yaw,cam.pitch,cam.fly?"true":"false",cam.swimming?"true":"false",
                manual_collision_steps,manual_slope_steps,manual_sliding_steps,513.*513.*4./1048576.,
                authored_state.source_files,authored_state.source_aliases,authored_state.source_colliders,
                starting_route.active?"true":"false",world_json::quote(starting_route.id()).c_str(),starting_route.length(),world_atlas_walk_speed(),1./60.,
                starting_route.walking_frames,starting_route.steps,starting_route.walking_frames/60.,
                starting_route.attempted,starting_route.actual,starting_route.next-1,starting_route.collision_blocked,
                starting_route.slope_blocked,starting_route.sliding_steps,starting_route.waiting_frames,
                starting_route.complete?"true":"false",starting_route.active?std::hypot(cam.x-starting_route.point(0)[0],cam.z-starting_route.point(0)[1]):0.);
            for(size_t i=0;i<source_reports.size();i++) {
                const auto &source=source_reports[i];
                std::fprintf(f,"    {\"path\": %s, \"instances\": %zu, \"trees\": %zu, \"props\": %zu, \"aliases\": %zu, \"colliders\": %zu}%s\n",
                    world_json::quote(source.path).c_str(),source.stats.instances,source.stats.trees,source.stats.props,source.stats.aliases,
                    source.stats.colliders,i+1<source_reports.size()?",":"");
            }
            std::fputs("  ]\n}\n",f);
            std::fclose(f);
        }

    }

    for(const auto portrait:arena_player_portraits)if(portrait)glDeleteTextures(1,&portrait);
    arena_player_portraits.clear();arena_player_portrait_attempted.clear();
    if(demo_logo)glDeleteTextures(1,&demo_logo);
    texture_cache_shutdown();
    land.free();
    water.free();
    post.free();
    foliage.free();
    foliageShadow.free();
    glDeleteTextures(10, materials);
    glDeleteTextures(1,&macro_height);
    glDeleteVertexArrays(1, &emptyVAO);
    editor_ui_shutdown(&ui);
    audio_shutdown();
    SDL_GL_DeleteContext(app.gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    world_atlas_shutdown();
    return launch->exit_code;
}

