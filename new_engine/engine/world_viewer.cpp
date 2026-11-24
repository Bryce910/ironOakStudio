#include "engine/world_scenery.h"
#include "engine/world_opening_interior.h"
#include "engine/world_studio.h"
#include "engine/world_json_string.h"
#include "engine/world_playtest_slice.h"
#include "engine/world_playtest_walk.h"
#include "engine/world_opening_prologue.h"
#include "engine/world_studio_scene_record.h"
#include "engine/world_opening_game_screen.h"
#include "engine/world_opening_cinematic_settings.h"
#include "engine/world_opening_cinematic_audio.h"
#include "engine/world_grandstone_qa_cases.h"
#include "engine/world_transform_math.h"
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
#include "engine/world_studio_tools.h"
#include "engine/world_assets.h"
#include "engine/world_environment.h"
#include <future>
#include <functional>
#include <filesystem>
#include <memory>
#include <chrono>
#include <climits>
#include <cfloat>
#include "engine/world_demo_contours.h"
#include "engine/world_combat_motion.h"
#include "engine/world_arena_combat.h"
#include "engine/world_arena_camera.h"
#include "engine/world_arena_render.h"
#include "engine/world_arena_bow_sockets.inc"
#include "engine/world_combat_vfx_render.h"
#include "engine/world_arena_progress.h"
#include "engine/world_blood_render.h"
#include "engine/world_presentation_voice.h"
#include "engine/world_castle_dragons.h"
#include "engine/world_castle_dragons_layout.h"
#include "engine/world_dragon_breath.h"
#include "engine/world_dragon_lights.h"
#include "engine/world_follow_camera.h"
#include "engine/world_presentation.h"
#include "engine/world_controller.h"
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
#include "engine/world_cover.h"
#include "engine/world_grass_transition.h"
#include "engine/world_temporal.h"
#include "engine/world_scenery.h"
#include "engine/world_water_scene.h"
#include "engine/world_sky.h"
#include "engine/world_occlusion.h"
#include "engine/world_bloom.h"
#include "engine/world_weather.h"
#include "engine/world_rainforest_atmosphere.h"
#include "engine/world_milestone.h"
#include "engine/world_jungle_expansion_data.h"
#include "engine/world_stronghold.h"
#include "engine/world_stronghold_data.h"
#include "engine/world_nature_grandstone.h"
#include "engine/world_snow.h"
#include "engine/world_forest_shelter.h"
#include "engine/world_pool_reflection_scissor.h"
#include "engine/world_pool_reflection_visibility.h"
#include "engine/world_selection_membership.h"
#include "engine/world_horizon_grid.h"
#include "engine/world_terrain_visibility.h"
#include "engine/world_height_region.h"
#include "engine/world_scene_particles.h"
#include "engine/world_vfx.h"
#include "engine/world_demo_audio.h"
#include "engine/world_uniform_cache.h"
#include "engine/world_local_probes.h"
#include "engine/world_gpu_budget_audit.h"
#include "engine/world_village_population.h"
#include "engine/world_castle_training.h"
#include "engine/world_training_combat.h"
#include "engine/world_village_watch.h"
#include "engine/world_wildlife.h"
#include "engine/world_bandit_camp.h"
#include "engine/world_trail_camp.h"
#include "engine/world_graphics_settings.h"
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
#include <sstream>
#include <iomanip>
#include <tuple>
#include <utility>
#include <unistd.h>

namespace {
    bool visual_surface_finish=false;std::array<float,4> visual_grass_interactor={};
    bool milestone_finish=false;
    bool stronghold_finish=false;
    bool source_showcase=false;
    bool city_showcase=false;
    world_city_scene::Look city_look;
    const WorldCitySky *active_city_sky=nullptr;
    const world_physics::World *active_city_physics=nullptr;
    const world_playtest_walk::Support *active_playtest_walk=nullptr;
    double playtest_walk_reference=0;
    struct V3 {
        float x, y, z;
    };

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

    struct Program {
        GLuint id = 0;
        WorldUniformCache loc;
        std::array<GLint,10> torch_uniforms={};
        bool torch_cached=false;
        Program()=default;
        Program(const Program &)=delete;
        Program &operator=(const Program &)=delete;
        Program(Program &&other)noexcept{*this=std::move(other);}
        Program &operator=(Program &&other)noexcept{
            if(this!=&other){free();id=std::exchange(other.id,0);loc=std::move(other.loc);torch_uniforms=other.torch_uniforms;torch_cached=other.torch_cached;other.loc.reset();other.torch_cached=false;}
            return *this;
        }
        GLint at(const char *n) {
            return loc.get(id,n,[&](const char *name){return glGetUniformLocation(id,name);});
        }

        bool load(const char *v, const char *f,bool reject_source=false,bool character=false) {
            loc.reset();
            torch_cached=false;
            // The measured scalar kernel remains the default. QA can compare
            // a separately compiled gather variant without imposing that
            // variant's register cost on ordinary play.
            const bool gather_trial=std::getenv("VALWOOD_QA_ENABLE_SHADOW_GATHER")&&
                !std::getenv("VALWOOD_QA_DISABLE_SHADOW_GATHER");
            char *vs=glsl_load(v),*fs=glsl_load(f);
            if(vs&&fs){
                std::string vertex(vs),fragment(fs),defines;
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
                if(fl!=std::string::npos)fragment.insert(fl+1,defines);
                id=GLH_program(vertex.c_str(),fragment.c_str());
            }
            std::free(vs);std::free(fs);
            GLint ok = 0;
            glGetProgramiv(id, GL_LINK_STATUS, &ok);
            if(ok&&studio_environment_features){GLint current=0;glGetIntegerv(GL_CURRENT_PROGRAM,&current);glUseProgram(id);glUniform1i(glGetUniformLocation(id,"uEnvironmentCube"),15);glUniform1i(glGetUniformLocation(id,"uEnvironmentBRDF"),6);glUseProgram(GLuint(current));}
            if(ok&&!studio_graph_code.surface.empty()){GLint current=0;glGetIntegerv(GL_CURRENT_PROGRAM,&current);glUseProgram(id);glUniform1i(glGetUniformLocation(id,"uStudioGraphIndex"),-1);glUseProgram(GLuint(current));}
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
            if(id)glDeleteProgram(id);
            id = 0;
            loc.reset();
        }
    };

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

    float sample_height(double x, double z, void *) {
        return world_atlas_height(x, z);
    }

    // A constrained imported scene uses its own geometry and reference storey;
    // an unsupported probe must never fall back to unrelated atlas terrain.
    bool active_opening_prologue=false,active_combat_arena=false;
    float view_near_plane(){return active_playtest_walk||active_opening_prologue||active_combat_arena?.12f:.4f;}
    float walking_height(double x, double z, void *) {
        if(active_playtest_walk){float floor=0;return active_playtest_walk->sample(x,z,playtest_walk_reference,floor)?floor:float(playtest_walk_reference);}
        return std::max(world_atlas_walk_height(x, z), 0.f);
    }

    struct WorldLiveVisuals;
    bool world_live_actor_move_clear(const WorldLiveVisuals*,const world_runtime::Runtime*,double,double,double,double,float,float,float);
    const WorldLiveVisuals *active_live_visuals=nullptr;
    const WorldStronghold *active_stronghold=nullptr;
    world_runtime::Runtime *active_world_runtime=nullptr;
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
            if(active_playtest_walk)return true;
            return world_walk_can_move(ax,az,bx,bz,walking_height,nullptr,1.2);
        };
        auto scenery_clear=[&](double ax,double az,double bx,double bz) {
            if(active_playtest_walk){
                if(cam.grounded){
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
            if(!world_stronghold_can_move(active_stronghold,ax,az,bx,bz,feet,.37f))return false;
            if(!world_live_actor_move_clear(active_live_visuals,active_world_runtime,ax,az,bx,bz,feet,1.75f,.37f))return false;
            if(active_world_runtime&&active_world_runtime->configured()){world_physics::Hit hit;world_runtime::Id entity=0;if(active_world_runtime->cast_capsule({ax,feet+.9,az},.37f,.50f,{bx-ax,0,bz-az},hit,entity)&&hit.hit&&hit.normal.y<.65&&hit.fraction<.999f)return false;}
            if(active_city_physics){world_physics::Hit hit;if(active_city_physics->cast_capsule({ax,feet+.9,az},.37f,.50f,{bx-ax,0,bz-az},hit)&&hit.hit&&hit.normal.y<.65&&hit.fraction<.999f)return false;}
            return true;
        };
        if(active_playtest_walk){
            // Each accepted axis owns its approved floor. Reusing the old feet
            // for the second axis creates false collisions beside real steps.
            auto attempt=[&](double tx,double tz){
                if(!scenery_clear(cam.x,cam.z,tx,tz))return false;
                cam.x=tx;cam.z=tz;
                if(cam.grounded){feet=approved_floor;cam.y=feet+1.75;}
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

        void draw(Program &p, const Camera &c, M4 vp, bool shadow = false) {
            p.use();
            p.m4("uVP", vp);
            for (unsigned i : draws) {
                GpuTile &t = tiles[i];
                double dx = t.x + t.size * .5 - c.x, dz = t.z + t.size * .5 - c.z;
                float radius = t.size * .72f;
                if (shadow && std::hypot(dx, dz) > 260 + radius)
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
               !std::strcmp(id,"start")||!std::strcmp(id,"courtyard")||!std::strcmp(id,"workshop")||
               !std::strcmp(id,"overlook")||!std::strcmp(id,"grove")||
               !std::strcmp(id,"rainforest")||!std::strcmp(id,"rainforest-deep")||!std::strcmp(id,"rainforest-pool")||
               !std::strcmp(id,"rainforest-purchased-understory")||!std::strcmp(id,"rainforest-camp")||!std::strcmp(id,"rainforest-milestone");
    }
    void scenic_travel(Camera &cam, unsigned i) {
        const auto &view = scenic_views[i];
        world_atlas_uv_to_world(view.u,view.v,&cam.x,&cam.z);
        cam.y = std::max(world_atlas_height(cam.x,cam.z),0.f)+(cam.fly ? 120. : view.eye_height);
        cam.yaw=view.yaw; cam.pitch=cam.fly ? -.22f : view.pitch; cam.vy=0;cam.grounded=!cam.fly;
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
    #include "engine/world_studio_view.inc"
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

    void light_program(Program &p, const Lighting &l,const Camera &camera) {
        studio_decals(p,camera);
        if(studio_environment&&studio_document){const auto &e=studio_document->environment;world_environment_bind(studio_environment,p.id,e.rotation,e.intensity*l.environment_gain,e.enabled?e.strength:0.f);}
        p.i("uSoftShadows",1);
        p.v3("uSun",l.sun);p.v3("uSunColor",l.sunColor);
        p.v3("uAmbient",l.ambient);p.v3("uFog",l.fog);
        p.v3("uSkyLight",l.skyLight);p.v3("uGroundLight",l.groundLight);
        p.v3("uSkyZenith",l.skyZenith);p.f("uHaze",l.haze);
        p.f("uCloud",l.cloud);p.f("uTime",l.time);
        glUniform4fv(p.at("uHomeRoomLights"),1,world_opening_room_lights::emission_weights.data());
        glUniform4fv(p.at("uHomeGameState"),1,world_opening_game_screen::state.data());
    }

    void torch_program(Program &p,const WorldTorchLights &lights) {
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

    void weather_program(Program &p,float wx,float wz,float wet) {
        p.v2("uWeatherWind",wx,wz);p.f("uWetness",stronghold_finish?0.f:wet);studio_surface_gpu.bind_wind(p);p.i("uVegetationFinish",visual_surface_finish?1:0);glUniform4fv(p.at("uGrassInteractor"),1,visual_grass_interactor.data());
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
        const auto *pool=world_atlas_rainforest_pool();
        f.pool_bounds[0]=float(pool->x);f.pool_bounds[1]=float(pool->z);
        f.pool_bounds[2]=float(pool->radius_x);f.pool_bounds[3]=float(pool->radius_z);f.pool_level=float(pool->surface_y);
        f.surface_height=weather_surface;return f;
    }

    WorldSceneParticlesFrame scene_particle_frame(const Camera &cam,const Lighting &light,V3 right,V3 up,V3 forward,
                                                   const M4 &vp,const M4 &light_vp,int width,int height,
                                                   unsigned color,unsigned depth,unsigned shadow) {
        WorldSceneParticlesFrame f;f.near_plane=view_near_plane();f.tan_half_fov=std::tan(cam.fov*.5f);
        f.color=color;f.depth=depth;f.shadow=shadow;f.width=width;f.height=height;
        f.camera[0]=cam.x;f.camera[1]=cam.y;f.camera[2]=cam.z;f.time=light.time;
        std::memcpy(f.vp,vp.m,sizeof(vp.m));std::memcpy(f.light_vp,light_vp.m,sizeof(light_vp.m));
        auto copy=[](float *out,V3 v){out[0]=v.x;out[1]=v.y;out[2]=v.z;};
        copy(f.right,right);copy(f.up,up);copy(f.forward,forward);copy(f.sun,light.sun);copy(f.sun_color,light.sunColor);
        copy(f.sky_light,light.skyLight);copy(f.ground_light,light.groundLight);copy(f.fog,light.fog);
        f.haze=light.haze;f.wind_xz[0]=.16f;f.wind_xz[1]=.05f;if(!studio_surface_gpu.data.winds.empty()){f.local_wind=WorldSurfaceGPU::particle_wind;f.wind_userdata=&studio_surface_gpu.data;}return f;
    }

    WorldSkyFrame sky_frame(const Camera &cam,const Lighting &light,V3 right,V3 up,V3 forward,float jx,float jy,bool source_background=true) {
        WorldSkyFrame frame;
        auto copy=[](float *out,V3 v){out[0]=v.x;out[1]=v.y;out[2]=v.z;};
        copy(frame.right,right);copy(frame.up,up);copy(frame.forward,forward);
        copy(frame.eye,{float(cam.x),float(cam.y),float(cam.z)});copy(frame.sun,light.sun);copy(frame.sun_color,light.sunColor);
        copy(frame.fog,light.fog);copy(frame.ambient,light.ambient);copy(frame.sky_light,light.skyLight);
        copy(frame.ground_light,light.groundLight);copy(frame.sky_zenith,light.skyZenith);
        frame.aspect=cam.aspect;frame.tan_half_fov=std::tan(cam.fov*.5f);frame.cloud=light.cloud;frame.time=light.time;
        frame.jitter_ndc[0]=jx;frame.jitter_ndc[1]=jy;if(studio_environment&&studio_document){auto stats=world_environment_stats(studio_environment);const auto &e=studio_document->environment;frame.environment_texture=stats.raw;frame.environment_rotation=e.rotation;frame.environment_intensity=e.intensity*light.environment_gain;frame.environment_strength=e.enabled?e.strength:0.f;}
        if(source_background&&city_showcase&&active_city_sky&&active_city_sky->ready()){
            frame.environment_texture=active_city_sky->texture;frame.environment_rotation=0;frame.environment_intensity=1;frame.environment_strength=1;
            frame.cloud=0;frame.sun_disc=false;
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
        bool init(const WorldScenery *scenery) {
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
                glGenTextures(1,&city_coverage);glBindTexture(GL_TEXTURE_2D_ARRAY,city_coverage);
                GLint unpack_alignment=4;glGetIntegerv(GL_UNPACK_ALIGNMENT,&unpack_alignment);glPixelStorei(GL_UNPACK_ALIGNMENT,1);
                glTexImage3D(GL_TEXTURE_2D_ARRAY,0,GL_R8,int(coverage.columns),int(coverage.rows),surrounding>0?2:1,0,GL_RED,GL_UNSIGNED_BYTE,pixels.data());
                glPixelStorei(GL_UNPACK_ALIGNMENT,unpack_alignment);
                glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
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
            if(!forest_pool||!forest_shelter.build(scenery,forest_pool->x+22,forest_pool->z+25,side,forest_pixels,
                [](double x,double z){return world_atlas_height(x,z);}))return false;
            // The shelter field's formerly unused alpha carries authored rock
            // exposure. Same allocation and same terrain sample; no new image.
            for(int z=0;z<side;z++)for(int x=0;x<side;x++){
                const double lx=-70.+140.*x/(side-1),lz=-145.+270.*z/(side-1);
                forest_pixels[(std::size_t(z)*side+x)*4+3]=(unsigned char)std::lround(world_demo_rock_exposure(lx,lz)*255);
            }
            std::copy(forest_pixels.begin(),forest_pixels.end(),pixels.begin()+std::size_t(count+2)*side*side*4);
            std::printf("[World] Forest shelter: %u authored crowns, min%.3f mean%.3f sky, one shared513x513 layer\n",
                forest_shelter.trees,forest_shelter.minimum_access,forest_shelter.mean_access);
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
            if(source_showcase)return masks&&starting;
            if(!detail.load_litter("ground/terrain_dirt_nor.png","ground/terrain_dirt_rgh.png",
                visual_surface_finish?"ground/forest_litter_color.png":nullptr,visual_surface_finish?"ground/forest_litter_normal.png":nullptr,visual_surface_finish?"ground/forest_litter_material.png":nullptr))return false;
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
            p.v2("uForestShelterHeights",WorldForestShelter::height_min,WorldForestShelter::height_range);
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
            p.i("uCityGroundMaskReady",city_coverage?1:0);p.i("uCityGroundCoverage",13);
            glUniform4fv(p.at("uCityGroundBounds"),1,city_ground_bounds.data());
            p.i("uCountrysideGroundMaskReady",city_coverage&&countryside_ground_ready?1:0);
            glUniform4fv(p.at("uCountrysideGroundBounds"),1,countryside_ground_bounds.data());
            if(city_coverage){glActiveTexture(GL_TEXTURE13);glBindTexture(GL_TEXTURE_2D_ARRAY,city_coverage);}

        }
        void release() {world_height_region_clear(&source_roofs);detail.release();GLuint textures[]={masks,starting,village_pbr,city_coverage};glDeleteTextures(4,textures);}
    };

    struct GrassGPU {
        GLuint vao = 0, vbo = 0, ebo = 0, instances = 0;
        GLsizei count = 0;
        size_t blades = 0,culled_frustum=0,culled_wall=0;
        double x = 0, z = 0;
        std::vector<float> staging;
        std::vector<unsigned char> procedural_keep;
        std::uint64_t classification_revision=0;
        std::uint64_t classification_refreshes=0,classified_seeds=0;
        bool has_classification=false;
        void init() {
            staging.reserve(WORLD_COVER_GRASS_CAP*2*6);
            procedural_keep.reserve(WORLD_COVER_GRASS_CAP*2);
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
            auto &data=staging;data.clear();culled_frustum=culled_wall=0;
            // Opacity-only fades keep root bytes/order unchanged. Reclassify
            // only after replacement/reset/compaction, never for fade progress.
            if(!has_classification||classification_revision!=root_revision||procedural_keep.size()!=f.count) {
                procedural_keep.resize(f.count);
                classification_refreshes++;classified_seeds+=f.count;
                for(size_t i=0;i<f.count;i++) {
                    const auto &a=f.instances[i];
                    procedural_keep[i]=!(std::sin(a.yaw*733.1f+a.scale*91.7f)>0);
                }
                classification_revision=root_revision;has_classification=true;
            }
            const auto view=world_vegetation_visibility_view(cam.x,cam.y,cam.z,cam.yaw,cam.pitch,cam.aspect,std::tan(cam.fov*.5f));
            double ox,oz;world_atlas_village_origin(&ox,&oz);
            const auto wall=world_castle_occlusion_view(cam.x-ox,cam.y,cam.z-oz);
            for (size_t i = 0; i < f.count; i++) {
                const auto &a = f.instances[i];
                // Short inexpensive blades form a meadow understory between
                // photographed tufts. A rooted blade keeps the same seeded
                // selection across cache-anchor changes at every distance.
                if(!procedural_keep[i]) continue;
                if(fades[i]<=0.f)continue;
                const double wx=f.anchor_x+a.x,wz=f.anchor_z+a.z;
                const double radius=.5*a.scale+.25,top=a.y+.27*a.scale+.15;
                if(world_vegetation_sphere_outside(view,wx,(a.y+top)*.5,wz,radius,90.)){culled_frustum++;continue;}
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
            glUniform4f(p.at("uBaseFactor"), .13f, .25f, .038f, 1.f);
            p.f("uWind", 1.6f);
            glBindVertexArray(vao);
            glDrawElementsInstanced(GL_TRIANGLES, count, GL_UNSIGNED_INT, nullptr, GLsizei(blades));
            p.i("uInstanceFade",0);
        }

        void release() {
            glDeleteVertexArrays(1, &vao);
            GLuint buffers[] = {vbo, ebo, instances};
            glDeleteBuffers(3, buffers);
        }
    };

    struct Dressing {
        static constexpr size_t scanned_batch_capacity=4096,scanned_group_capacity=8192;
        struct Group {
            WorldModelAsset *asset = nullptr,*shadow_asset=nullptr,*shadow_cull_asset=nullptr,*overflow_asset=nullptr;
            WorldCoverKind kind = WORLD_COVER_TREES;
            unsigned variant = 0, variants = 1;
            float near_distance = 0, far_distance = 0;
            size_t cap = 0;
            bool shadow = false;
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
        size_t scanned_grass_count = 0;
        size_t grass_blend_updates=0,peak_blended_grass_roots=0;
        size_t peak_scanned_group_candidates=0,scanned_grass_cap_drops=0,scanned_culled_frustum=0,scanned_culled_wall=0;
        size_t tree_count = 0, near_tree_count = 0, mid_tree_count = 0, far_tree_count = 0;
        uint64_t tree_dispatches=0,grass_dispatches=0,tree_fallbacks=0,grass_fallbacks=0;
        bool add(const char *path, WorldCoverKind kind, unsigned variant, unsigned variants,
                 float lo, float hi, size_t cap, bool shadow, int vegetation) {
            Group g;
            g.asset = world_model_load(path);
            // Two bounded draw batches retain both rooted generations during
            // a grass transition. Embedded images share the texture cache.
            if(kind==WORLD_COVER_GRASS&&cap>scanned_batch_capacity)
                g.overflow_asset=world_model_load(path);
            float minimum[3],maximum[3];
            if(world_model_bounds(g.asset,minimum,maximum)) {
                g.tree_height=maximum[1]-minimum[1];g.bound_top=maximum[1];
                g.bound_radius=std::hypot(std::max(std::abs(minimum[0]),std::abs(maximum[0])),
                                          std::max(std::abs(minimum[2]),std::abs(maximum[2])));
            }
            g.kind = kind; g.variant = variant; g.variants = variants;
            g.near_distance = lo; g.far_distance = hi; g.cap = cap;
            g.shadow = shadow; g.vegetation = vegetation;
            g.visible.reserve(cap); g.uploaded.reserve(cap);g.shadow_selected.reserve(cap);g.shadow_uploaded.reserve(cap);
            if(kind==WORLD_COVER_GRASS){g.visible_fades.reserve(cap);g.uploaded_fades.reserve(cap);}
            g.selected.reserve(kind == WORLD_COVER_TREES ? WORLD_COVER_TREE_CAP :
                               kind==WORLD_COVER_GRASS?std::max(cap,size_t(WORLD_COVER_GRASS_CAP*2/12+256)):512);
            groups.push_back(std::move(g));
            return groups.back().asset != nullptr&&
                (kind!=WORLD_COVER_GRASS||cap<=scanned_batch_capacity||groups.back().overflow_asset!=nullptr);
        }
        static bool upload(Group &g,const std::vector<WorldModelInstance> &instances,const std::vector<float> &fades) {
            if(g.kind==WORLD_COVER_GRASS) {
                if(instances.size()>scanned_group_capacity||instances.size()!=fades.size())return false;
                const size_t first=std::min(instances.size(),scanned_batch_capacity),rest=instances.size()-first;
                if(rest&&!g.overflow_asset)return false;
                bool ready=world_model_upload_fading_instances(g.asset,instances.data(),fades.data(),first);
                if(g.overflow_asset)ready=world_model_upload_fading_instances(g.overflow_asset,
                    rest?instances.data()+first:nullptr,rest?fades.data()+first:nullptr,rest)&&ready;
                return ready;
            }
            bool ready=world_model_upload_instances(g.asset,instances.data(),instances.size());
            if(g.shadow_asset)ready=world_model_upload_instances(g.shadow_asset,instances.data(),instances.size())&&ready;
            if(g.shadow_cull_asset)ready=world_model_upload_instances(g.shadow_cull_asset,instances.data(),instances.size())&&ready;
            if(ready&&(g.shadow_asset||g.shadow_cull_asset))g.shadow_uploaded.assign(instances.begin(),instances.end());
            return ready;
        }
        bool init() {
            // The publisher scene supplies its own foliage. Avoid retaining
            // the unused atlas forest, grass and their reflection/shadow copies.
            if(source_showcase)return true;
            cover = world_cover_init();
            shadow_lod=std::getenv("VALWOOD_DISABLE_SHADOW_LOD")==nullptr;
            tree_species.reserve(WORLD_COVER_TREE_CAP);
            grass.init();
            groups.reserve(24);
            bool ok = cover != nullptr;
            const char *species[] = {"pine", "fir","broadleaf","pine_young"};
            for(unsigned i=0;i<4;i++) {
                char path[128];std::snprintf(path,sizeof(path),"models/cinematic_%s_far.glb",species[i]);
                reflection_assets[i]=world_model_load(path);
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
                        groups.back().shadow_cull_asset=world_model_load(path);
                        ok=groups.back().shadow_cull_asset&&ok;
                    }
                    if(lod==0) {
                        std::snprintf(path,sizeof(path),"models/cinematic_%s_mid.glb",species[type]);
                        groups.back().shadow_asset=world_model_load(path);
                        ok=groups.back().shadow_asset&&ok;
                    }
                }
            for (unsigned variant = 0; variant < 3; variant++) {
                char path[128];
                std::snprintf(path, sizeof(path), "models/cinematic_rock_%u.glb", variant);
                ok = add(path, WORLD_COVER_ROCKS, variant, 3, 0, 200, 60, true, 0) && ok;
                std::snprintf(path, sizeof(path), "models/woodland_fern_%u.glb", variant);
                ok = add(path, WORLD_COVER_FERNS, variant, 3, 0, 110, 120, false, 3) && ok;
            }
            for(unsigned variant=0;variant<3;variant++) {
                char path[128];std::snprintf(path,sizeof(path),"models/cinematic_grass_%u.glb",variant);
                ok=add(path,WORLD_COVER_GRASS,variant*4,12,0,70,scanned_group_capacity,false,5)&&ok;
            }
            return ok;
        }
        static uint32_t root_hash(const WorldCoverField &, const WorldModelInstance &a) {
            // These world-seeded attributes are identical across cover anchors.
            // Reconstructing global coordinates from local floats can change a
            // quantized position at a cell boundary and switch species in place.
            uint32_t yaw,scale;std::memcpy(&yaw,&a.yaw,sizeof(yaw));std::memcpy(&scale,&a.scale,sizeof(scale));
            uint64_t h = uint64_t(yaw) * 0x9e3779b97f4a7c15ull ^ uint64_t(scale) * 0xbf58476d1ce4e5b9ull;
            h ^= h >> 30; h *= 0xbf58476d1ce4e5b9ull; h ^= h >> 27;
            return uint32_t(h ^ (h >> 32));
        }
        static unsigned species(uint32_t root,float wet) {
            const unsigned choice=root%10;
            if(wet>0&&float(root>>8)*(1.f/16777216.f)<wet)
                return choice<5?2:(choice<9?1:3);
            return choice<4?0:(choice<7?1:(choice<9?2:3));
        }
        void update(const Camera &cam,double seconds=1./60.) {
            if(source_showcase)return;
            world_cover_update(cover, cam.x, cam.z);
            world_cover_pump(cover, 4096);
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
                        const float wet=world_atlas_rainforest_weight(field.anchor_x+a.x,field.anchor_z+a.z);
                        tree_species.push_back(static_cast<unsigned char>(species(root,wet)));
                    }
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
            if(source_showcase)return;
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
            scanned_culled_frustum=scanned_culled_wall=0;
            tree_count = near_tree_count = mid_tree_count = far_tree_count = scanned_grass_count = 0;
            V3 forward = cam.forward(), right = cam.right();
            const auto *pool=world_atlas_rainforest_pool();
            const bool pool_mirror=std::hypot(cam.x-pool->x,cam.z-pool->z)<220.;
            // Dispatch the same candidate pairs in original source order. A
            // malformed/extended group layout retains the old per-group scan.
            std::array<std::array<Group*,3>,4> tree_groups={};
            std::array<Group*,12> grass_groups={};
            bool tree_dispatch=tree_species.size()==fields[WORLD_COVER_TREES].count,grass_dispatch=true;
            double grass_lo=1e300,grass_hi=-grass_lo;
            for(auto &g:groups) {
                g.selected.clear();
                if(g.kind==WORLD_COVER_TREES) {
                    if(g.variant>=4||g.variants!=4||g.lod<0||g.lod>=3)tree_dispatch=false;
                    else {auto &slot=tree_groups[g.variant][std::size_t(g.lod)];if(slot)tree_dispatch=false;else slot=&g;}
                } else if(g.kind==WORLD_COVER_GRASS) {
                    if(g.variant>=12||g.variants!=12||!std::isfinite(g.near_distance)||!std::isfinite(g.far_distance))grass_dispatch=false;
                    else {
                        auto &slot=grass_groups[g.variant];if(slot)grass_dispatch=false;else slot=&g;
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
                    if(fades[i]<=0.f)continue;
                    const auto &a=f.instances[i];
                    const double x=f.anchor_x+a.x-cam.x,z=f.anchor_z+a.z-cam.z,d2=x*x+z*z;
                    if(d2>=grass_hi||d2<grass_lo)continue;
                    auto *g=grass_groups[root_hash(f,a)%grass_groups.size()];if(!g)continue;
                    if(d2>=g->far_distance*g->far_distance||d2<g->near_distance*g->near_distance)continue;
                    const double wx=f.anchor_x+a.x,wz=f.anchor_z+a.z;
                    const double radius=g->bound_radius*a.scale*1.4+.25,top=a.y+g->bound_top*a.scale*1.4+.15;
                    if(world_vegetation_sphere_outside(grass_view,wx,(a.y+top)*.5,wz,
                        std::hypot(radius,(top-a.y)*.5),70.)){scanned_culled_frustum++;continue;}
                    if(studio_castle_culling&&world_castle_wall_occludes(grass_wall,wx-village_x-radius,a.y-.05,wz-village_z-radius,
                        wx-village_x+radius,top,wz-village_z+radius)){scanned_culled_wall++;continue;}
                    if(!g->shadow&&!point_visible(a,x,z))continue;
                    g->selected.push_back({d2,i});
                }
            }
            for (auto &g : groups) {
                const auto &f = fields[g.kind];
                g.visible.clear();g.visible_fades.clear();
                if((g.kind!=WORLD_COVER_TREES||!tree_dispatch)&&(g.kind!=WORLD_COVER_GRASS||!grass_dispatch)) {
                    for (size_t i = 0; i < f.count; i++) {
                        const auto &a = f.instances[i];
                        if(g.kind==WORLD_COVER_GRASS&&grass_transition.fades()[i]<=0.f)continue;
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
                                std::hypot(radius,(top-a.y)*.5),70.)){scanned_culled_frustum++;continue;}
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
                if(g.kind==WORLD_COVER_GRASS)scanned_grass_count+=g.visible.size();
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
            if(source_showcase)return;
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
            if(source_showcase)return;
            auto &f=fields[WORLD_COVER_TREES];
            p.v3("uOffset",{float(f.anchor_x-cam.x),float(-cam.y),float(f.anchor_z-cam.z)});
            p.v2("uWorldAnchor",float(f.anchor_x),float(f.anchor_z));
            p.i("uGrass",0);p.i("uVegetation",4);p.i("uLodLevel",-1);p.f("uWind",.4f);
            for(unsigned i=0;i<4;i++) {p.f("uTreeHeight",reflection_heights[i]);world_model_draw(reflection_assets[i],p.id);}
        }
        bool prepare_probe_trees() {
            if(source_showcase)return true;
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
            if(source_showcase)return true;
            bool ready=true;
            for(unsigned i=0;i<4;i++)ready=world_model_upload_instances(reflection_assets[i],reflection_uploaded[i].data(),reflection_uploaded[i].size())&&ready;
            return ready;
        }
        void draw(Program &p, const Camera &cam, bool shadows = false) {
            if(source_showcase)return;
            p.i("uGrass", 0);
            for (auto &g : groups) {
                if (shadows && !g.shadow) continue;
                const auto &f = fields[g.kind];
                p.v3("uOffset", {float(f.anchor_x-cam.x),float(-cam.y),float(f.anchor_z-cam.z)});
                p.v2("uWorldAnchor",float(f.anchor_x),float(f.anchor_z));
                p.i("uVegetation",g.vegetation);
                p.i("uLodLevel",g.lod);
                p.f("uTreeHeight",g.tree_height);
                p.f("uWind",g.kind == WORLD_COVER_ROCKS ? 0.f : .75f);
                auto *asset=shadows&&shadow_lod?(g.shadow_asset?g.shadow_asset:(tree_shadow_culling&&g.shadow_cull_asset?g.shadow_cull_asset:g.asset)):g.asset;
                if(asset)world_model_draw(asset,p.id);
                if(!shadows&&g.overflow_asset)world_model_draw(g.overflow_asset,p.id);
            }
            if (!shadows) grass.draw(p,cam);
        }
        void release() {
            world_cover_shutdown(cover);
            for(auto &g:groups){world_model_free(g.asset);world_model_free(g.shadow_asset);world_model_free(g.shadow_cull_asset);world_model_free(g.overflow_asset);}
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
                b.asset=world_model_load(i<4 ? "models/cinematic_pine_far.glb" : "models/cinematic_fir_far.glb");
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
                    if(studio_document&&!studio_document->stamps.empty())probability*=world_studio::vegetation(*studio_document,1,px,pz);
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
            asset=world_model_load("models/cinematic_cliff_01.glb");
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
struct TrainingSounds {
    std::array<Mix_Chunk*,4> clips={};int first=-1;unsigned events=0,body_events=0,metal_events=0;bool ready=false;
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
        if(first<0||first>128||Mix_AllocateChannels(first+4)!=first+4){first=-1;release();return false;}
        ready=true;return true;
    }
    void play(unsigned clip,int volume){
        if(!ready||clip>=clips.size())return;
        const int channel=first+int(clip);Mix_HaltChannel(channel);Mix_Volume(channel,volume);
        const int played=clip==0?Mix_FadeInChannel(channel,clips[clip],0,4):Mix_PlayChannel(channel,clips[clip],0);
        if(played>=0){events++;body_events+=clip==1;metal_events+=clip==2;}
    }
    void mute(){if(ready){for(int i=0;i<4;++i)Mix_Volume(first+i,0);}}
    void release(){
        if(first>=0){for(int i=0;i<4;++i)Mix_HaltChannel(first+i);if(Mix_AllocateChannels(-1)==first+4)Mix_AllocateChannels(first);first=-1;}
        for(auto *&c:clips){if(c)Mix_FreeChunk(c);c=nullptr;}ready=false;
    }
};
}

extern "C" int world_viewer_run(EngineLaunch *launch, EngineProject *project) {
    const bool presentation=std::getenv("VALWOOD_PRESENTATION")!=nullptr&&!launch->editor;
    const bool opening_enabled=presentation&&std::getenv("VALWOOD_OPENING_PROLOGUE")&&std::filesystem::is_regular_file("opening.config");
    const bool first_journey_enabled=opening_enabled&&std::filesystem::is_regular_file("opening/first-journey.settings");
    const bool first_journey_qa=first_journey_enabled&&launch->frames&&!launch->benchmark&&std::getenv("VALWOOD_QA_FIRST_JOURNEY");
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
    if(std::filesystem::is_regular_file("opening.config")&&!opening_enabled){std::fprintf(stderr,"[Opening] Use the guarded opening launcher. Open the original workspace for authoring.\n");return 1;}
    active_opening_prologue=opening_enabled;
    struct OpeningClipScope {~OpeningClipScope(){active_opening_prologue=false;}} opening_clip_scope;
    const bool qa_town=presentation&&launch->frames&&!launch->benchmark&&std::getenv("VALWOOD_QA_TOWN");
    const bool qa_controller=presentation&&launch->frames&&!launch->benchmark&&std::getenv("VALWOOD_QA_CONTROLLER");
    visual_surface_finish=std::filesystem::is_regular_file("visual_upgrade.json");visual_grass_interactor={};
    milestone_finish=std::filesystem::is_regular_file("milestone_standard.json");
    stronghold_finish=std::filesystem::is_regular_file("stronghold.json");
    source_showcase=std::filesystem::is_regular_file("publisher_showcase.json");
    city_showcase=std::filesystem::is_regular_file("city_showcase.json");
    const char *playtest_area=std::getenv("VALWOOD_PLAYTEST_SLICE");
    const bool playtest_enabled=playtest_area&&*playtest_area;
    const bool arena_enabled=presentation&&playtest_enabled&&std::getenv("VALWOOD_COMBAT_ARENA")&&std::filesystem::is_regular_file("arena.config");
    const bool arena_qa=arena_enabled&&launch->frames&&!launch->benchmark&&std::getenv("VALWOOD_QA_ARENA");
    active_combat_arena=arena_enabled;
    struct ArenaScope {~ArenaScope(){active_combat_arena=false;}} arena_scope;
    constexpr double arena_origin_x=-1800.,arena_origin_z=-10500.,arena_floor=64.;
    if(std::filesystem::exists("arena.config")&&!arena_enabled){std::fprintf(stderr,"[Arena] Use the combat arena launcher.\n");return 1;}
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
    struct PlaytestWalkScope {~PlaytestWalkScope(){active_playtest_walk=nullptr;}} playtest_walk_scope;
    active_playtest_walk=nullptr;
    if(playtest_enabled){
        const bool matching=(!std::strcmp(playtest_area,"city")&&city_showcase)||
            (!std::strcmp(playtest_area,"jungle")&&std::filesystem::is_regular_file("jungle_expansion.json"))||
            (!std::strcmp(playtest_area,"grandstone")&&stronghold_finish&&source_showcase);
        world_playtest_slice::Path route;std::string error;
        if(!presentation||launch->editor||!matching||!world_playtest_slice::load("playtest.route",route,error)||!playtest_state.start(route,&error)){
            std::fprintf(stderr,"[Playtest] Matching project, play mode and valid route required: %s\n",error.c_str());return 1;
        }
        playtest_walk_reference=route.spawn.eye_y-1.75;
        if(stronghold_finish){
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
        if(std::filesystem::exists("city.physics")){
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
    WorldGraphicsSettings graphics_draft=graphics,graphics_pending=graphics;
    bool show_graphics=launch->graphics_menu,graphics_apply=false,display_resize=false;
    int display_w=launch->width,display_h=launch->height;
    uint64_t quality_epoch=1,quality_valid_after=120,quality_reallocations=0,quality_apply_failures=0;
    QualityController quality;quality.reset({graphics.auto_quality,graphics.render_scale,graphics.min_render_scale,float(graphics.target_fps)},quality_epoch);
    float rendered_scale=graphics.auto_quality?quality.scale():graphics.render_scale;
    float minimum_rendered_scale=rendered_scale,maximum_rendered_scale=rendered_scale;

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
    if(!launch->frames&&!graphics_display(window,graphics)){
        std::snprintf(graphics_message,sizeof(graphics_message),"Saved display mode unavailable; using a window on this display.");
        SDL_SetWindowFullscreen(window,0);
    }
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
    Program land, water, post, foliage, foliageShadow;WorldLivePrograms live_programs;
    const char *qa_terrain_fragment=std::getenv("VALWOOD_QA_TERRAIN_FRAGMENT");
    const char *qa_foliage_fragment=std::getenv("VALWOOD_QA_FOLIAGE_FRAGMENT");
    bool shaders = land.load("shaders/world_terrain.vs.glsl",
        qa_terrain_fragment&&*qa_terrain_fragment?qa_terrain_fragment:"shaders/world_terrain.fs.glsl") &&
        water.load("shaders/world_water.vs.glsl", "shaders/world_water.fs.glsl") && post.load("shaders/world_post.vs.glsl",
        "shaders/world_post.fs.glsl") && foliage.load("shaders/world_foliage.vs.glsl",
        qa_foliage_fragment&&*qa_foliage_fragment?qa_foliage_fragment:"shaders/world_foliage.fs.glsl") && foliageShadow.load("shaders/world_foliage.vs.glsl",
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
            next_foliage.load("shaders/world_foliage.vs.glsl",qa_foliage_fragment&&*qa_foliage_fragment?qa_foliage_fragment:"shaders/world_foliage.fs.glsl")&&
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
        const auto ids=[&](){return std::array<GLuint,10>{land.id,water.id,foliage.id,foliageShadow.id,live_programs.skeletal.id,live_programs.skeletal_shadow.id,live_programs.rigid.id,live_programs.rigid_shadow.id,live_programs.crowd.id,live_programs.crowd_shadow.id};};
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
        std::printf("[Character program QA] %s:10 unique owners, post-link rollback, moved-from cleanup, failed link, successful commit and isolated crowd variants\n",ok?"PASS":"FAIL");
        if(!ok){live_programs.free();land.free();water.free();post.free();foliage.free();foliageShadow.free();editor_ui_shutdown(&ui);audio_shutdown();SDL_GL_DeleteContext(app.gl_context);SDL_DestroyWindow(window);SDL_Quit();world_atlas_shutdown();return 1;}
    }

    GLuint macro_height=0;
    GLuint materials[10] = {
        texture("ground/terrain_grass.png"), texture("ground/terrain_rock.png"), texture("ground/terrain_sand.png"), texture("ground/terrain_dirt.png"), texture("ground/terrain_grass_nor.png",
            false), texture("ground/terrain_rock_nor.png", false), climate_texture(macro_height), 0, texture("ground/terrain_grass_rgh.png", false), texture("ground/terrain_rock_rgh.png", false)
    };
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
    WorldBloom *bloom=world_bloom_create("shaders/world_post.vs.glsl","shaders/world_bloom_extract.fs.glsl",
        "shaders/world_bloom_blur.fs.glsl");
    const bool bloom_enabled=std::getenv("VALWOOD_QA_DISABLE_BLOOM")==nullptr;
    const float bloom_strength=bloom_enabled?.035f:0.f;
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
    world_milestone::Review milestone_review;Camera milestone_return_camera;
    int milestone_return_screen=0;bool milestone_return_third=true,milestone_return_save=false;
    const char *milestone_return_name=nullptr;float portrait_focus_distance=0;
    // A capture-only override leaves ordinary play's default and preferences unchanged.
    const char *capture_view=std::getenv("VALWOOD_CAPTURE_VIEW");
    const bool force_capture_first=launch->frames&&capture_view&&!std::strcmp(capture_view,"first");
    bool third_person=!force_capture_first;
    unsigned camera_toggles=0;float boom_distance=3.6f;
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
    for (size_t i=0; i<sizeof(scenic_views)/sizeof(scenic_views[0]); i++)
        if (!std::strcmp(launch->scenic,scenic_views[i].id)) {
            scenic_travel(cam,unsigned(i)); current_name=scenic_views[i].name;
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
    if(city_showcase&&!source_viewpoint){
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
    if(access("scenery/demo_completion_instances.txt",F_OK)==0)scenery_files.push_back("scenery/demo_completion_instances.txt");
    WorldSceneryStreamingConfig scenery_config;
    if(source_showcase){scenery_config.geometry_bytes=1280ull*1024*1024;scenery_config.texture_bytes=1280ull*1024*1024;scenery_config.cpu_bytes=768ull*1024*1024;scenery_config.preload_meters=64;scenery_config.unload_meters=96;}
    // The imported city houses prepare their full model and shadow proxy
    // together. Allow that transient CPU pair without enlarging GPU budgets.
    if(city_showcase)scenery_config.cpu_bytes=1280ull*1024*1024;
    if(opening_interior.enabled&&opening_interior.release_city_fallbacks){
        scenery_config.interior_release_city_fallbacks=true;
        scenery_config.interior_start_active=world_opening_interior::contains(opening_interior,{cam.x,cam.y,cam.z});
        for(unsigned k=0;k<3;k++){scenery_config.interior_minimum[k]=opening_interior.bounds.minimum[k];scenery_config.interior_maximum[k]=opening_interior.bounds.maximum[k];}
    }
    auto load_initial_scenery=[&](){return std::getenv("VALWOOD_QA_DISABLE_ASSET_STREAMING")?world_scenery_load_files(scenery_files.data(),std::size(scenery_files),"models",scenery_error,sizeof scenery_error):world_scenery_load_files_streamed(scenery_files.data(),std::size(scenery_files),"models",cam.x,cam.z,scenery_config,scenery_error,sizeof scenery_error);};
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
    size_t early_gpu_submitted_frames=0;
    const bool pool_reflection_scissor_enabled=!(launch->frames&&std::getenv("VALWOOD_QA_DISABLE_POOL_REFLECTION_SCISSOR"));
    const bool pool_reflection_clip_enabled=!(launch->frames&&std::getenv("VALWOOD_QA_DISABLE_POOL_REFLECTION_CLIP"));
    uint64_t pool_reflection_clip_tests=0,pool_reflection_clip_outside=0,pool_reflection_clip_skips=0;
    const bool wildlife_preparation_enabled=!(launch->frames&&std::getenv("VALWOOD_QA_DISABLE_WILDLIFE_PREPARATION"));
    size_t pool_reflection_scissor_frames=0;
    uint64_t pool_reflection_geometry_pixels=0,pool_reflection_full_pixels=0;
    valid=landscape.init(scenery)&&valid;
    startup_status("authored scenery and landscape materials");
    if(valid&&!studio_surface_gpu.prepare(landscape.masks,studio.live.surfaces,studio.message)){std::fprintf(stderr,"[Studio] Surface library rejected: %s\n",studio.message.c_str());studio.live.surfaces={};studio.dirty=studio.open=true;valid=reload_studio_shaders(studio_shader_features,studio_environment_features,&studio.live)&&valid;}
    char population_error[256]={};
    const bool population_enabled=!city_showcase&&!stronghold_finish&&std::getenv("VALWOOD_DISABLE_VILLAGE_POPULATION")==nullptr;
    WorldVillagePopulation *population=population_enabled?world_village_population_load("models",population_error,sizeof(population_error)):nullptr;
    if(population_enabled&&!population)std::fprintf(stderr,"[World] Village population: %s\n",population_error);
    valid=(!population_enabled||population)&&valid;
    studio.population_ptr=population;studio.materials(scenery);
    {Studio::RuntimePrepared prepared;std::string error;if(studio.prepare_runtime(studio.live,scenery,prepared,error)&&studio.commit_runtime(prepared,scene_particles,torch_flames)){}else{studio.message="Saved runtime edits need repair: "+error;studio.open=studio.dirty=true;studio.live.actors.clear();studio.live.navigation.clear();studio.live.particles.clear();world_village_population_configure(population,nullptr,0);world_scene_particles_set_emitters(scene_particles,scene_emitters.data(),scene_emitters.size());world_torch_flames_set_emitters(torch_flames,flame_emitters.data(),flame_emitters.size());}}
    studio.memory_appearance_init();
    std::string stronghold_error;
    WorldStronghold *stronghold=stronghold_finish?world_stronghold_load("models/stronghold",stronghold_error):nullptr;
    if(stronghold_finish&&!stronghold){std::fprintf(stderr,"[Grandstone] %s\n",stronghold_error.c_str());valid=false;}
    active_stronghold=stronghold;
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
    unsigned combat_contact_kind=0,combat_vfx_arrow_hits=0;
    float combat_contact_time=0,combat_contact_damage=0;
    world_arena_combat::Arena arena_game;WorldArenaRender *arena_renderer=nullptr;
    WorldCombatVfxRenderer *combat_vfx_renderer=world_combat_vfx_create();valid=(combat_vfx_renderer!=nullptr)&&valid;
    char motion_error[256]={};
    const char *motion_directory=std::getenv("VALWOOD_QA_COMBAT_DIRECTORY");
    const bool combat_motion_enabled=population_enabled||playtest_enabled||first_journey_enabled;
    const bool character_polish_enabled=(arena_enabled||first_journey_enabled)&&std::filesystem::is_regular_file("CHARACTER_POLISH.json");
    arena_game.keep_defeated=character_polish_enabled;
    WorldCombatMotion *combat_motion=combat_motion_enabled?world_combat_motion_load(motion_directory&&*motion_directory?motion_directory:"models/longsword_mocap",motion_error,sizeof motion_error,character_polish_enabled):nullptr;
    if(combat_motion_enabled&&!combat_motion)std::fprintf(stderr,"[World] Combat motion: %s\n",motion_error);
    valid=(!combat_motion_enabled||combat_motion)&&valid;
    if(combat_motion_enabled)valid=training_weapon.init()&&valid;
    if(arena_enabled){
        char error[256]={};arena_renderer=world_arena_render_create({arena_origin_x,arena_floor+.02,arena_origin_z,12,12},error,sizeof error);
        const bool ok=arena_renderer&&world_combat_motion_prepare_crowd(combat_motion)&&world_arena_render_load_bow(arena_renderer,"arena_assets",arena_bow_sockets,error,sizeof error);
        if(!ok)std::fprintf(stderr,"[Arena] Load failed: %s\n",error);valid=ok&&valid;
    }
    if(first_journey_enabled)valid=character_polish_enabled&&blood_enabled&&world_combat_motion_prepare_crowd(combat_motion)&&valid;
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
    if(wildlife&&access("jungle_expansion.json",F_OK)==0)valid=world_wildlife_expand_jungle(wildlife)&&valid;
    valid=wildlife&&valid;
    size_t wildlife_peak_visible=0;
    int wildlife_pending=-2;
    WorldBanditCamp *bandits=world_bandit_camp_create("models");
    valid=bandits&&valid;
    size_t bandit_peak_visible=0;
    int bandit_pending=-2;
    WorldTrailCamp *arrival_camp=world_trail_camp_create("models");
    WorldTrailCamp *first_journey_townsfolk=nullptr;
    bool arrival_camp_enabled=false;int arrival_camp_pending=-1;char arrival_camp_error[256]={};
    valid=arrival_camp&&valid;
    WorldTrailCamp *trail_camp=world_trail_camp_create("models");
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
    if(launch->frames&&studio.path.rfind("/tmp/",0)==0){if(const char *tool=std::getenv("VALWOOD_QA_PRODUCTION_TOOL")){char *end=nullptr;const long value=std::strtol(tool,&end,10);if(end&&!*end&&value>=14&&value<=24){studio.open=true;studio.requested_tab=int(value);}}}
    if(launch->frames&&studio.path.rfind("/tmp/",0)==0){if(const char *view=std::getenv("VALWOOD_QA_RUNTIME_VIEW")){
        double tx=0,tz=0;float ty=0;bool found=false;
        if(!std::strcmp(view,"water")){for(unsigned i=0;i<studio.live.waters.size();i++){const auto &w=studio.live.waters[i];if(w.source!=3)continue;tx=w.x;tz=w.z;ty=float(w.level);studio.water_selection=int(i);studio.requested_tab=12;found=true;break;}}
        if(!std::strcmp(view,"npc")&&!studio.live.actors.empty()){const auto &a=studio.live.actors[0];tx=a.points[0][0];tz=a.points[0][1];ty=world_atlas_walk_height(tx,tz)+.8f;studio.actor_selection=0;studio.area_selection=0;studio.requested_tab=10;if(!studio.live.navigation.empty())world_navigation::bake(studio.live.navigation[0],studio.nav_query(scenery),studio.navigation_preview,studio.message);found=true;}
        if(!std::strcmp(view,"particles")&&!studio.live.particles.empty()){const auto &p=studio.live.particles.back();tx=p.x;tz=p.z;ty=float(p.y+.7);studio.particle_selection=int(studio.live.particles.size()-1);studio.requested_tab=11;found=true;}
        if(found){cam.x=tx+9;cam.z=tz+10;cam.y=std::max(double(world_atlas_height(cam.x,cam.z))+2.5,double(ty)+4);cam.yaw=std::atan2(-9.f,10.f);cam.pitch=std::atan2(float(ty-cam.y),std::hypot(9.f,10.f));cam.fly=true;cam.grounded=false;studio.open=!std::getenv("VALWOOD_QA_RUNTIME_CLEAN");third_person=false;current_name="Runtime editor inspection";}
    }}
    if(combat_motion){world_combat_motion_update(combat_motion,fight,cam.x,cam.y-1.75,cam.z,3.14159265359f-cam.yaw,0,false);valid=training_weapon.prepare(combat_motion,target.w,target.h)&&valid;}
    if(dragons_enabled&&!commit_dragons(world_castle_dragons_desired(dragons,cam.x,cam.z))){
        std::fprintf(stderr,"[World] %s\n",dragons_error);valid=false;
    }
    if(!update_dragon_emitters(17.f))valid=false;
    char wildlife_error[256]={};
    if(wildlife&&!world_wildlife_commit(wildlife,city_showcase?-1:world_wildlife_loading_desired(wildlife,cam.x,cam.z),wildlife_error,sizeof(wildlife_error))) {
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
        dressing.update(cam);
        WorldCoverMetrics cover_metrics={};world_cover_metrics(dressing.cover,&cover_metrics);
        if(!cover_metrics.near_building&&!cover_metrics.tree_building) break;
    }
    horizon.update(cam,400000);
    world_scenery_update(scenery,cam.x,cam.y,cam.z,cam.yaw,cam.pitch,cam.aspect,std::tan(cam.fov*.5f));
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
    bool running = valid, show_map = false, look_active = false, show_tools = !launch->no_hud&&!launch->editor, destination_loading = false;
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
        post.f("uExposure", exposure);
        post.f("uDetailSharpness",studio.live.look.sharpness);
        post.f("uPortraitFocus",portrait_focus_distance);
        post.i("uTemporalActive",temporal?1:0);
        glActiveTexture(GL_TEXTURE2);glBindTexture(GL_TEXTURE_3D,village_grade.texture);
        post.i("uVillageGrade",2);
        post.f("uVillageGradeWeight",village_grade_ready?grade_strength*(city_showcase?1.f:float(world_atlas_village_weight(cam.x,cam.z))):0.f);
        post.v2("uResolution", float(target.w), float(target.h));
        glActiveTexture(GL_TEXTURE3);glBindTexture(GL_TEXTURE_2D,glow);post.i("uBloom",3);
        post.f("uBloomStrength",glow?bloom_strength:0.f);
        glBindVertexArray(emptyVAO);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glViewport(0,0,display_w,display_h);
    };
    #include "world_demo_probe_capture.inc"
    #include "world_runtime_host.inc"
    #include "world_demo_journey.inc"
    bool opening_controls_enabled=false,opening_movement_armed=false;
    world_first_journey::State first_journey_state;
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
    #include "world_presentation_runtime.inc"
    world_controller::SwordStick sword_stick;
    if(presentation&&(stronghold_finish||city_showcase)){preview_screen=1;preview_pending_save=false;preview_chapter_time=0;third_person=false;if(!city_showcase)cam.fly=source_showcase&&source_viewpoint;preview_release_mouse();}
    // A player-facing showcase launch bypasses the title and keeps the
    // existing journey untouched. QA controls remain separately frame-gated.
    std::function<bool()> arena_begin;
    std::function<bool(SDL_Keycode)> arena_key;
    std::function<void(const Camera&,bool)> arena_draw;
    #include "world_playtest_runtime.inc"
    #include "world_combat_arena_runtime.inc"
    #include "world_arena_qa.inc"
    #include "world_sword_stick_qa.inc"
    #include "world_opening_prologue_runtime.inc"
    #include "world_first_journey_runtime.inc"
    // The opening host has now applied its actual interior lighting values.
    if(running&&opening_enabled&&opening_interior.enabled&&!ensure_local_probes(cam)){launch->exit_code=1;running=false;}
    #include "world_playtest_combat_qa.inc"
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
            cam.fly=false;cam.grounded=true;third_person=false;avatar_initialized=false;
            destination_loading=true;temporal_cut=true;
        }
    }
    running=valid;
    // Startup and play exercise exactly the same reflection/material paths.
    // Startup uses no timing queries or walking-frame counters.
    auto draw_water_reflection=[&](const Camera &cam,const Lighting &light,const WorldTorchLights &torch_lights,
            const M4 &lightVP,float visual_time,float water_level,float wind_x,float wind_z,float wetness,
            float motion_delta,M4 &reflection_vp,bool record_stats,uint64_t frame)->bool {
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
        if(water_level==float(reflection_pool->surface_y)) {
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
        dressing.draw_reflected(foliage,mirror);horizon.draw(foliage,mirror);outcrops.draw(foliage,mirror);
        foliage.i("uSourceMaterialAO",city_showcase?1:0);
        world_scenery_draw(scenery,foliage.id,mirror.x,mirror.y,mirror.z);
        foliage.i("uSourceMaterialAO",0);
        if(world_village_population_stats(population).visible){prepare_reflected_foliage_program(live_programs.crowd);draw_population_color(live_programs.crowd,mirror,reflection_vp);foliage.use();}
        world_castle_training_draw(training,foliage.id,mirror.x,mirror.y,mirror.z);
        WorldTorchLights reflected_actor_lights;
        if(!source_actor_practicals(mirror,visual_time,reflected_actor_lights))return false;
        if(!cam.fly){
            if(source_flames_enabled){torch_program(foliage,reflected_actor_lights);foliage.i("uActorLighting",1);}
            world_combat_motion_draw(combat_motion,foliage.id,mirror.x,mirror.y,mirror.z,false);
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
        const world_studio::Water *style=custom;
        if(!style)for(const auto &w:studio.live.waters)if(w.source==source){style=&w;break;}
        if(style&&(!style->enabled||!world_entity::visible(studio.live.entities,{world_entity::Kind::Water,unsigned(style-studio.live.waters.data())})))return false;
        water.v3("uStudioWaterTint",style?V3{style->tint[0],style->tint[1],style->tint[2]}:V3{1,1,1});water.v3("uStudioWaterAbsorption",style?V3{style->absorption[0],style->absorption[1],style->absorption[2]}:V3{1,1,1});glUniform4f(water.at("uStudioWaterParams"),style?style->roughness:1,style?style->waves:1,style?style->flow:1,style?style->foam:1);water.f("uTime",time*(style?style->flow:1));return true;
    };
    auto draw_water_layers=[&](const Camera &cam,const Lighting &light,const WorldTorchLights &torch_lights,
            const M4 &lightVP,const M4 &vp,const M4 &reflection_vp,float visual_time,float water_level,
            float wind_x,float wind_z,float wetness,float rain_intensity,const float *jitter,
            bool reflect_water,bool draw_pool,bool draw_creek,bool record_stats)->bool {
        const V3 forward=cam.forward(),right=cam.right(),up=norm(cross(right,forward));
        const auto *pool=world_atlas_rainforest_pool();
        const GLenum color_buffers[]={GL_COLOR_ATTACHMENT0,GL_COLOR_ATTACHMENT1};
        bool custom_water=studio_surface_gpu.rivers.visible(cam.x,cam.z);for(const auto &w:studio.live.waters)if(w.source==3&&w.enabled&&world_entity::visible(studio.live.entities,{world_entity::Kind::Water,unsigned(&w-studio.live.waters.data())})&&std::hypot(cam.x-w.x,cam.z-w.z)<220)custom_water=true;
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
        light_program(water, light,cam);torch_program(water,torch_lights);
        weather_program(water,wind_x,wind_z,wetness);water.f("uRainIntensity",rain_intensity);
        water.i("uCastleCreek",0);water.i("uLocalPool",0);water.f("uWaterLevel",0);
        water.m4("uVP", vp);water.m4("uLightVP",lightVP);water.i("uShadow",7);
        water.v3("uOffset", {0, float(-cam.y), 0});
        water.v2("uOrigin", float(cam.x), float(cam.z));
        glActiveTexture(GL_TEXTURE6);
        glBindTexture(GL_TEXTURE_2D,materials[6]);
        water.i("uClimate",6);
        water.i("uHasOpaque",water_scene_ready?1:0);
        water.v2("uResolution",float(target.w),float(target.h));
        water.v3("uForward",forward);water.v3("uRight",right);water.v3("uUp",up);
        water.f("uEyeHeight",float(cam.y));
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
        if(bind_water_style(0,nullptr,visual_time))glDrawArrays(GL_TRIANGLES, 0, 6);
        if(draw_pool) {
            if(record_stats){pool_frames++;if(reflect_water)pool_reflection_frames++;}
            water.i("uLocalPool",1);water.f("uWaterLevel",float(pool->surface_y));
            glUniform4f(water.at("uPoolBounds"),float(pool->x),float(pool->z),float(1/pool->radius_x),float(1/pool->radius_z));
            water.i("uHasReflection",reflect_water&&water_level==float(pool->surface_y)?1:0);
            water.v3("uReflectionShift",{0,float(2*(cam.y-pool->surface_y)),0});
            if(bind_water_style(1,nullptr,visual_time))glDrawArrays(GL_TRIANGLES,0,6);
            water.i("uLocalPool",0);water.f("uWaterLevel",0);
        }
        if(draw_creek&&!city_showcase) {
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
        if(custom_water)for(const auto &w:studio.live.waters)if(w.source==3&&w.enabled&&world_entity::visible(studio.live.entities,{world_entity::Kind::Water,unsigned(&w-studio.live.waters.data())})&&std::hypot(cam.x-w.x,cam.z-w.z)<220){water.i("uLocalPool",1);water.i("uCastleCreek",0);water.i("uHasReflection",0);water.f("uWaterLevel",float(w.level));glUniform4f(water.at("uPoolBounds"),float(w.x),float(w.z),1/w.radius_x,1/w.radius_z);bind_water_style(3,&w,visual_time);glDrawArrays(GL_TRIANGLES,0,6);}
        for(const auto &r:studio_surface_gpu.rivers.ranges)if(WorldSurfaceRivers::nearby(r,cam.x,cam.z)){
            water.i("uRiverSurface",1);water.i("uLocalPool",1);water.i("uCastleCreek",0);water.i("uHasReflection",0);water.f("uWaterLevel",r.level);
            if(bind_water_style(4,nullptr,visual_time)){glBindVertexArray(studio_surface_gpu.rivers.vao);glDrawArrays(GL_TRIANGLES,r.first,r.count);}
        }
        water.i("uRiverSurface",0);

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
    if(running&&((!city_showcase&&(castle_creek_nearby(cam)||world_atlas_village_weight(cam.x,cam.z)>0.f))||startup_pool_nearby)) {
        const Uint64 begin=SDL_GetPerformanceCounter();
        bool ready=true;
        if(graphics.reflections) {
            ready=reflection.resize(std::max(1,int(std::lround(target.w*graphics.reflection_scale))),
                                    std::max(1,int(std::lround(target.h*graphics.reflection_scale))));
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
                    world_scenery_update(scenery,warm.x,warm.y,warm.z,warm.yaw,warm.pitch,warm.aspect,std::tan(warm.fov*.5f));
                    M4 reflectedVP;
                    const float level=startup_pool_nearby?float(world_atlas_rainforest_pool()->surface_y):float(WORLD_CASTLE_CREEK_SURFACE_Y);
                    ready=draw_water_reflection(warm,light,torch_lights,lightVP,visual_time,
                        level,0,0,0,qa_fixed_time?0.f:1.f/60.f,reflectedVP,false,0);
                    if(ready) {
                        glBindFramebuffer(GL_FRAMEBUFFER,target.fbo);glViewport(0,0,target.w,target.h);
                        const GLenum buffers[]={GL_COLOR_ATTACHMENT0,GL_COLOR_ATTACHMENT1};glDrawBuffers(2,buffers);
                        glDepthMask(GL_TRUE);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
                        const M4 vp=product(perspective(warm.fov,warm.aspect,view_near_plane(),15000.f),look({0,0,0},warm.forward(),{0,1,0}));
                        const float jitter[]={0,0};
                        ready=draw_water_layers(warm,light,torch_lights,lightVP,vp,reflectedVP,visual_time,
                            level,0,0,0,0,jitter,true,startup_pool_nearby,!startup_pool_nearby,false);
                    }
                    if(ready)creek_water_startup_passes++;
                }
                world_scenery_update(scenery,cam.x,cam.y,cam.z,cam.yaw,cam.pitch,cam.aspect,std::tan(cam.fov*.5f));
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
                const auto f0=scene_particle_frame(warm,light,r,u,f,vp,lightvp,target.w,target.h,target.color,target.depth,shadow.depth);
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
        const GLuint glow=world_bloom_resolve(bloom,target.color,target.w,target.h,exposure);
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
    char demo_audio_error[256]={};
    WorldDemoAudio *demo_audio=running?world_demo_audio_create("audio/demo",demo_audio_error,sizeof(demo_audio_error)):nullptr;
    if(combat_motion&&!training_sounds.init())std::fprintf(stderr,"[Training] Optional combat sounds unavailable\n");
    arena_shot_sound=[&](){training_sounds.play(0,25);};
    if(presentation){
        if(!preview_audio.init())std::fprintf(stderr,"[Preview] Optional presentation score unavailable\n");
        if(!preview_voices.init())std::fprintf(stderr,"[Preview] Optional recorded dialogue could not initialize\n");
        std::printf("[Preview] Human dialogue recordings available: %u/4\n",preview_voices.available);
        SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER);
        if(qa_controller||qa_town||opening_qa||qa_stick_combat||arena_qa){
            if(!controller_test_pad.attach()){std::fprintf(stderr,"[Controller QA] Virtual device failed: %s\n",SDL_GetError());launch->exit_code=1;running=false;}
            else preview_pad.prefer(SDL_JoystickInstanceID(controller_test_pad.joystick));
        }else preview_pad.open();
        preview_sync_controller_ui();
        studio.components_show_dialogue=false;show_tools=false;journey_open=false;
    }
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
    Uint64 last = SDL_GetPerformanceCounter();
    uint64_t frame = 0;
    #include "world_runtime_live_qa.inc"
    #include "world_presentation_qa.inc"
    #include "world_milestone_qa.inc"
    #include "world_controller_qa.inc"
    #include "world_town_qa.inc"
    #include "world_opening_prologue_qa.inc"
    #include "world_first_journey_qa.inc"
    FoliageCaptureSequence foliage_sequence;
    FoliageCaptureSequence npc_sequence{std::getenv("VALWOOD_NPC_SEQUENCE"),true,360,{}};
    double benchmark_x = cam.x, benchmark_z = cam.z;
    float benchmark_yaw = cam.yaw;
    double startup_ms = millis(startup);
    size_t peak_grass = 0, peak_trees = 0, loading_frames = 0, loading_streak = 0, longest_loading_streak = 0;
    size_t peak_near=0,peak_mid=0,peak_local_cards=0,peak_horizon=0,peak_horizon_roots=0,peak_scanned_grass=0;
    size_t manual_collision_steps=0,manual_slope_steps=0,manual_sliding_steps=0;
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
    std::printf("[World] Initial camera %s; V switches first/third person.\n",third_person?"third person":"first person");
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
    engine_perf_begin(launch->profile, launch->warmup);
    engine_perf_set_gpu_stage_timing_available(profile_gpu);
    while (running && !launch->finished) {
        // QA advances simulation at 60 Hz. Pace only its owner thread as well,
        // so uncapped loading frames leave the asset worker real wall time.
        // Ordinary play and all replay assertions/bounds remain unchanged.
        if(opening_qa){
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
        float dt = opening_qa?1.f/60.f:std::min(float(quality_dt), .05f);
        last = frame_start;
        if (quality_dt > .00001)
            fps = fps * .94f + float(.06 / quality_dt);
        frame++;
        bool source_review_waiting_frame=false;
        if(studio.designer_exit_approved){running=false;break;}
        sync_designer_camera();refresh_designer_view();
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
        town_workflow();
        controller_workflow();
        preview_workflow();
        milestone_workflow();
        playtest_input_workflow(frame);
        playtest_combat_workflow(frame);
        sword_stick_workflow(frame);
        arena_workflow(frame);
        grandstone_obstacle_workflow(frame);
        if(!first_journey_qa_handoff())opening_prologue_workflow(frame);
        first_journey_workflow(frame);
        bool pad_attack_requested=false,pad_thrust_requested=false;
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            // Integration replays only its tagged input events. Physical
            // desktop input must not trigger atlas travel or edit controls in
            // a deterministic QA run; normal play keeps ordinary SDL input.
            if(qa_training||qa_live||qa_preview||playtest_input_qa||playtest_combat_qa||grandstone_obstacle_qa||opening_qa||qa_stick_combat||arena_qa){
                if((event.type==SDL_KEYDOWN||event.type==SDL_KEYUP)&&event.key.keysym.unused!=0x56414c54u&&!((opening_qa||qa_stick_combat||arena_qa)&&event.key.keysym.unused==0x504144u))continue;
                if((event.type==SDL_MOUSEBUTTONDOWN||event.type==SDL_MOUSEBUTTONUP)&&event.button.which!=0x56414c54u)continue;
                if((event.type==SDL_MOUSEMOTION&&!(arena_qa&&event.motion.which==0x56414c54u))||event.type==SDL_MOUSEWHEEL||event.type==SDL_TEXTINPUT)continue;
                if(grandstone_obstacle_qa&&(event.type==SDL_CONTROLLERAXISMOTION||event.type==SDL_CONTROLLERBUTTONDOWN||event.type==SDL_CONTROLLERBUTTONUP))continue;
            }
            if((opening_qa||qa_stick_combat||arena_qa)&&(event.type==SDL_CONTROLLERAXISMOTION||event.type==SDL_CONTROLLERBUTTONDOWN||event.type==SDL_CONTROLLERBUTTONUP)&&event.cbutton.which!=preview_pad.id())continue;
            const bool pad_command=event.type==SDL_KEYDOWN&&event.key.keysym.unused==0x504144u;
            // Translated gameplay actions are one-shot engine commands, not
            // held keyboard keys. Feeding them to ImGui without a key-up could
            // leave a combat direction repeating inside a later pause menu.
            if(!pad_command)editor_ui_process_event(&ui, &event);
            if(presentation){
                if(!pad_command&&preview_pad.event(event)){
                    if(preview_screen==1)preview_screen=2;
                    preview_release_mouse();preview_notice="Controller disconnected. Reconnect or use keyboard and mouse.";preview_notice_time=8;
                }
                if(event.type==SDL_CONTROLLERDEVICEADDED||event.type==SDL_CONTROLLERDEVICEREMOVED)preview_sync_controller_ui();
                if(event.type==SDL_CONTROLLERBUTTONDOWN&&event.cbutton.which==preview_pad.id()&&preview_pad.focused){
                    if(opening_enabled&&opening_state.stage()==world_opening_prologue::Stage::GameMenu&&event.cbutton.button==SDL_CONTROLLER_BUTTON_START){
                        preview_screen=preview_screen==1?2:1;opening_release();continue;
                    }
                    if(!playtest_enabled&&!opening_enabled&&source_showcase&&preview_screen==1&&!studio.open&&!show_graphics&&!show_map&&!fight.engaged()&&
                       (event.cbutton.button==SDL_CONTROLLER_BUTTON_LEFTSHOULDER||event.cbutton.button==SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)&&!source_views.empty()){
                        if(event.cbutton.button==SDL_CONTROLLER_BUTTON_LEFTSHOULDER)source_view_index=(source_view_index+source_views.size()-2)%source_views.size();
                        next_demo_stop();preview_pad.using_pad=true;continue;
                    }
                    if(arena_enabled&&event.cbutton.button==SDL_CONTROLLER_BUTTON_RIGHTSHOULDER){if(arena_controls_ready())arena_toggle_lock();continue;}
                    if(arena_enabled&&arena_controls_ready()&&(event.cbutton.button==SDL_CONTROLLER_BUTTON_DPAD_LEFT||event.cbutton.button==SDL_CONTROLLER_BUTTON_DPAD_RIGHT)){arena_weapon(event.cbutton.button==SDL_CONTROLLER_BUTTON_DPAD_LEFT?world_arena_combat::Weapon::Sword:world_arena_combat::Weapon::Bow);continue;}
                    using A=world_controller::Action;
                    const auto action=world_controller::route(SDL_GameControllerButton(event.cbutton.button),preview_controller_context());
                    SDL_Keycode key=0;
                    switch(action){
                        case A::Back:key=SDLK_ESCAPE;break;
                        case A::Interact:key=SDLK_e;break;
                        case A::Jump:case A::Dodge:key=SDLK_SPACE;break;
                        case A::Journal:key=SDLK_j;break;
                        case A::Camera:key=SDLK_v;break;
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
                if(studio.designer_should_confirm_close()){studio.designer_close_requested=true;studio.open=true;studio.cancel_designer_drag();studio.designer_asset_cancel();look_active=false;SDL_SetRelativeMouseMode(SDL_FALSE);}
                else running=false;
                continue;
            }
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                if(opening_enabled){opening_release();opening_confirm=opening_interact=opening_cancel=opening_enter=false;}
                look_active = false;fight.set_guard(false);fight.clear_buffer();if(arena_enabled){arena_modal_neutral();arena_game.cancel_draw();arena_mouse_draw=arena_mouse_aim=false;arena_bow_armed=false;}
                if(presentation&&preview_screen==1&&!launch->frames)preview_screen=2;
                studio.cancel_designer_drag();
                SDL_SetRelativeMouseMode(SDL_FALSE);
            }
            if(event.type==SDL_WINDOWEVENT&&event.window.event==SDL_WINDOWEVENT_FOCUS_GAINED)relative_mouse_compatibility.warm(window);
            if(event.type==SDL_WINDOWEVENT&&event.window.event==SDL_WINDOWEVENT_FOCUS_GAINED&&!studio.open&&fight.engaged()&&(!presentation||!preview_pad.using_pad)&&!show_map&&!show_graphics&&!studio.production_freeze()&&!studio.cinematic_preview){look_active=true;SDL_SetRelativeMouseMode(SDL_TRUE);}
            if (event.type == SDL_WINDOWEVENT &&(event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED||event.window.event==SDL_WINDOWEVENT_DISPLAY_CHANGED)) {
                int w, h;
                SDL_GL_GetDrawableSize(window, &w, &h);
                if (w > 0 && h > 0) {
                    studio.cancel_designer_drag();
                    display_w=w;display_h=h;display_resize=true;
                    SDL_GetWindowSize(window,&logical_w,&logical_h);refresh_designer_view();
                    quality_valid_after=frame+60;

                }
            }
            if(opening_enabled&&(event.type==SDL_KEYDOWN||event.type==SDL_KEYUP)&&!pad_command&&!event.key.repeat){
                const bool down=event.type==SDL_KEYDOWN;
                const auto k=event.key.keysym.sym;
                if(k==SDLK_RETURN||k==SDLK_KP_ENTER||k==SDLK_SPACE)opening_confirm=down;
                if(k==SDLK_RETURN||k==SDLK_KP_ENTER)opening_enter=down;
                if(k==SDLK_e)opening_interact=down;
                if(k==SDLK_r&&!down)first_journey_retry_down=false;
                if(k==SDLK_ESCAPE)opening_cancel=down;
            }
            if(opening_enabled&&event.type==SDL_MOUSEBUTTONDOWN&&opening_controls_enabled&&!show_graphics&&!destination_loading&&!launch->frames){look_active=true;preview_pad.using_pad=false;SDL_SetRelativeMouseMode(SDL_TRUE);}
            if (event.type == SDL_KEYDOWN && !launch->benchmark &&
                (!launch->frames || qa_training || qa_live || qa_preview || qa_milestone || qa_controller || qa_town || opening_qa || playtest_input_qa || playtest_combat_qa || qa_stick_combat || arena_qa || (launch->frames&&studio.path.rfind("/tmp/",0)==0&&(std::getenv("VALWOOD_QA_DESIGNER_WORKSPACE")||std::getenv("VALWOOD_QA_EDITOR_COPY_PASTE")||std::getenv("VALWOOD_QA_EDITOR_TERRAIN_SNAP")||std::getenv("VALWOOD_QA_EDITOR_VFX"))) || (qa_map_cycle && event.key.keysym.sym==SDLK_TAB)) &&
                !event.key.repeat && !ImGui::GetIO().WantTextInput) {
                int k = event.key.keysym.sym;
                if(preview_key(SDL_Keycode(k)))continue;
                if(arena_enabled&&!arena_controls_ready()&&k!=SDLK_F10)continue;
                if(k==SDLK_F11&&!ImGui::IsAnyItemActive()){studio.cancel_designer_drag();studio.designer_asset_cancel();studio.open=!studio.open;show_tools=false;show_map=show_graphics=false;fight.leave();look_active=false;SDL_SetRelativeMouseMode(SDL_FALSE);sync_designer_camera();refresh_designer_view();continue;}
                if(studio.open){
                    // Authoring owns these keys before gameplay. A focused text
                    // or numeric widget keeps its own shortcuts and keystrokes.
                    if(k==SDLK_ESCAPE&&(studio.drag.active||studio.designer_asset_placing()||(!ImGui::IsAnyItemActive()&&!studio.designer_blocks_viewport()))){studio.cancel_designer_drag();studio.designer_asset_cancel();studio.vfx_placing=false;show_graphics=false;look_active=false;SDL_SetRelativeMouseMode(SDL_FALSE);}
                    using Key=world_designer_viewport::Key;Key key=Key::None;
                    switch(k){case SDLK_q:key=Key::Q;break;case SDLK_w:key=Key::W;break;case SDLK_e:key=Key::E;break;case SDLK_r:key=Key::R;break;case SDLK_f:key=Key::F;break;case SDLK_p:key=Key::P;break;case SDLK_c:key=Key::C;break;case SDLK_v:key=Key::V;break;case SDLK_d:key=Key::D;break;case SDLK_s:key=Key::S;break;case SDLK_z:key=Key::Z;break;case SDLK_y:key=Key::Y;break;case SDLK_DELETE:key=Key::Delete;break;case SDLK_BACKSPACE:key=Key::Backspace;break;case SDLK_END:key=Key::End;break;default:break;}
                    const auto modifiers=event.key.keysym.mod;world_designer_viewport::ShortcutContext context;
                    context.text_input=ImGui::GetIO().WantTextInput;context.active_widget=ImGui::IsAnyItemActive()||studio.drag.active;context.blocked=studio.designer_blocks_viewport()||studio.production_freeze()||studio.cinematic_preview;context.navigating=look_active;
                    context.ctrl=(modifiers&KMOD_CTRL)!=0;context.shift=(modifiers&KMOD_SHIFT)!=0;context.alt=(modifiers&KMOD_ALT)!=0;context.super=(modifiers&KMOD_GUI)!=0;
                    world_designer_viewport::Command command=world_designer_viewport::Command::Select;
                    if(world_designer_viewport::shortcut(key,context,command))studio.designer_command(command);
                    continue;
                }
                if(studio.production_freeze()||studio.cinematic_preview)continue;
                if(k==SDLK_j){journey_open=!journey_open;look_active=false;SDL_SetRelativeMouseMode(SDL_FALSE);}
                if(k==SDLK_F5){save_journey();journey_open=true;}
                if(k==SDLK_F8){load_journey();journey_open=true;}
                if(k==SDLK_v){third_person=!third_person;camera_toggles++;temporal_cut=true;}
                if(presentation&&k==SDLK_SPACE&&!fight.engaged()&&!preview_blocked()&&!milestone_review.portrait()&&!destination_loading&&cam.grounded){cam.vy=4.7f;cam.grounded=false;}
                if(k==SDLK_e&&!studio.open&&!cam.fly&&!show_map&&!show_graphics&&!destination_loading&&studio.components_interact({cam.x,cam.y-1.75,cam.z})){look_active=false;SDL_SetRelativeMouseMode(SDL_FALSE);continue;}
                if(k==SDLK_e&&!studio.open&&!cam.fly&&!show_map&&!show_graphics&&!destination_loading&&training){
                    double ox,oz;world_atlas_village_origin(&ox,&oz);
                    const bool retry=!fight.engaged()&&fight.state!=TrainingState::Idle&&std::hypot(cam.x-ox-fight.enemy_x,cam.z-oz-fight.enemy_z)<8.;
                    if(retry&&studio.components_runtime&&studio.components_runtime->enqueue({world_gameplay::EventKind::Interact,world_runtime_demo::instructor_id})){look_active=false;SDL_SetRelativeMouseMode(SDL_FALSE);continue;}
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
                const bool eligible=!studio.drag.active&&!launch->frames&&!launch->benchmark&&!studio.production_freeze()&&!studio.cinematic_preview&&!studio.designer_blocks_viewport();
                if(event.type==SDL_MOUSEBUTTONUP&&event.button.button==SDL_BUTTON_RIGHT){look_active=false;SDL_SetRelativeMouseMode(SDL_FALSE);}
                else if(event.type==SDL_MOUSEBUTTONDOWN&&event.button.button==SDL_BUTTON_RIGHT&&eligible&&designer_view.contains(float(event.button.x),float(event.button.y))&&!editor_ui_wants_mouse(&ui)&&!ImGui::IsAnyItemActive()){
                    look_active=true;SDL_SetRelativeMouseMode(SDL_TRUE);
                }else if(event.type==SDL_MOUSEMOTION&&look_active&&eligible){
                    cam.yaw+=event.motion.xrel*.0022f;cam.pitch=std::clamp(cam.pitch-event.motion.yrel*.0022f,-1.48f,1.48f);
                }else if(event.type==SDL_MOUSEWHEEL&&eligible&&!editor_ui_wants_mouse(&ui)){
                    int mx=0,my=0;SDL_GetMouseState(&mx,&my);if(designer_view.contains(float(mx),float(my)))studio.fly_speed=std::clamp(studio.fly_speed*std::exp(event.wheel.y*.15f),1.f,1000.f);
                }
                continue;
            }
            if((preview_blocked()||studio.production_freeze()||studio.cinematic_preview)&&(event.type==SDL_MOUSEBUTTONDOWN||event.type==SDL_MOUSEBUTTONUP||event.type==SDL_MOUSEMOTION))continue;
            if(arena_enabled&&event.type==SDL_MOUSEBUTTONDOWN&&event.button.button==SDL_BUTTON_MIDDLE){if(arena_controls_ready())arena_toggle_lock();continue;}
            if(arena_enabled&&arena_controls_ready()&&arena_bow()&&(!launch->frames||arena_qa)&&preview_pad.focused&&!preview_blocked()&&!show_map&&!show_graphics&&!destination_loading){
                if(event.type==SDL_MOUSEBUTTONDOWN||event.type==SDL_MOUSEBUTTONUP){const bool down=event.type==SDL_MOUSEBUTTONDOWN;if(event.button.button==SDL_BUTTON_LEFT)arena_mouse_draw=down;if(event.button.button==SDL_BUTTON_RIGHT){arena_mouse_aim=down;look_active=down||arena_mouse_draw;SDL_SetRelativeMouseMode(look_active?SDL_TRUE:SDL_FALSE);}continue;}
                if(event.type==SDL_MOUSEMOTION&&(look_active||arena_mouse_draw||fight.engaged())){cam.yaw+=event.motion.xrel*.0022f*preview_settings.sensitivity;cam.pitch=std::clamp(cam.pitch-event.motion.yrel*.0022f*preview_settings.sensitivity,-1.1f,1.1f);continue;}
            }
            if((!arena_enabled||arena_controls_ready())&&fight.engaged()&&(!launch->frames||qa_training||qa_preview||qa_controller||playtest_combat_qa||qa_stick_combat||arena_qa)&&!launch->benchmark&&!show_map&&!show_graphics&&!destination_loading){
                if(event.type==SDL_MOUSEBUTTONDOWN){
                    if(event.button.button==SDL_BUTTON_LEFT)fight.attack();
                    if(event.button.button==SDL_BUTTON_MIDDLE){fight.choose(TrainingDirection::Thrust);fight.attack();}
                    if(event.button.button==SDL_BUTTON_RIGHT)fight.set_guard(true);
                    continue;
                }
                if(event.type==SDL_MOUSEBUTTONUP){if(event.button.button==SDL_BUTTON_RIGHT)fight.set_guard(false);continue;}
                if(event.type==SDL_MOUSEMOTION&&arena_look_held()){
                    cam.yaw+=event.motion.xrel*.0022f*preview_settings.sensitivity;
                    cam.pitch=std::clamp(cam.pitch-event.motion.yrel*.0022f*preview_settings.sensitivity*(preview_settings.invert_y?-1.f:1.f),-1.1f,1.1f);
                    fight.set_guard(false);fight.clear_buffer();continue;
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
        arena_refresh_input();
        sync_designer_camera();refresh_designer_view();
        if(studio.open&&studio.designer_frame_pending){
            studio.designer_frame_pending=false;
            if(studio.designer_frame_reveal){cam.pitch=std::clamp(cam.pitch,-.85f,-.45f);cam.yaw=studio.designer_frame_heading;studio.designer_frame_reveal=false;}
            const auto center=studio.designer_frame_center;const double radius=std::clamp(studio.designer_frame_radius,.25,100000.);
            if(std::isfinite(center.x)&&std::isfinite(center.y)&&std::isfinite(center.z)&&std::isfinite(radius)){
                cam.aspect=world_designer_viewport::aspect(designer_view);const double half=std::min(double(cam.fov*.5f),std::atan(std::tan(double(cam.fov*.5f))*cam.aspect));
                const double distance=radius/std::max(std::sin(half),.01)*1.18;const auto forward=cam.forward();
                cam.x=std::clamp(center.x-forward.x*distance,-world_atlas_width()*.5+1,world_atlas_width()*.5-1);
                cam.z=std::clamp(center.z-forward.z*distance,-world_atlas_depth()*.5+1,world_atlas_depth()*.5-1);cam.y=std::clamp(center.y-forward.y*distance,-12000.,12000.);
                cam.fly=true;cam.grounded=false;cam.vy=0;temporal_cut=true;
            }
        }
        if(studio.open&&!studio.drag.active&&!launch->frames&&!launch->benchmark&&look_active&&!studio.production_freeze()&&!studio.cinematic_preview&&!studio.designer_blocks_viewport()&&!ImGui::GetIO().WantTextInput&&!ImGui::IsAnyItemActive()){
            const auto *keys=SDL_GetKeyboardState(nullptr);float speed=studio.fly_speed*((keys[SDL_SCANCODE_LSHIFT]||keys[SDL_SCANCODE_RSHIFT])?4.f:1.f)*((keys[SDL_SCANCODE_LCTRL]||keys[SDL_SCANCODE_RCTRL])?.25f:1.f);
            const auto forward=cam.forward(),right=cam.right();const float f=float(keys[SDL_SCANCODE_W])-float(keys[SDL_SCANCODE_S]),s=float(keys[SDL_SCANCODE_D])-float(keys[SDL_SCANCODE_A]),v=float(keys[SDL_SCANCODE_E])-float(keys[SDL_SCANCODE_Q]);
            V3 delta=add(add(mul(forward,f),mul(right,s)),{0,v,0});const float length=std::sqrt(dot(delta,delta));if(length>1)delta=mul(delta,1/length);
            cam.x=std::clamp(cam.x+delta.x*speed*dt,-world_atlas_width()*.5+1,world_atlas_width()*.5-1);cam.z=std::clamp(cam.z+delta.z*speed*dt,-world_atlas_depth()*.5+1,world_atlas_depth()*.5-1);cam.y=std::clamp(cam.y+delta.y*speed*dt,-12000.,12000.);
            cam.fly=true;cam.grounded=false;cam.vy=0;
        }

        if(presentation&&preview_pad.using_pad&&look_active){look_active=false;SDL_SetRelativeMouseMode(SDL_FALSE);}
        const auto pad_context=preview_controller_context();
        const bool pad_playable=presentation&&preview_pad.focused&&!studio.open&&!preview_blocked()&&!show_map&&!show_graphics&&!destination_loading&&
            !world_controller::ui_owns(pad_context);
        preview_pad.poll(preview_settings.deadzone,pad_playable&&!milestone_review.active());
        if(pad_playable&&milestone_review.portrait()){
            milestone_review.drag(preview_pad.look.x*dt*450.f,preview_pad.look.y*dt*300.f*(preview_settings.invert_y?-1.f:1.f));
            milestone_review.zoom((preview_pad.right_trigger-preview_pad.left_trigger)*dt*8.f);
        }
        const bool player_controls_allowed=(!arena_enabled||arena_controls_ready())&&!milestone_review.portrait()&&!preview_blocked()&&!studio.open&&!(studio.components_runtime&&studio.components_runtime->current_line())&&!studio.production_freeze()&&!studio.cinematic_preview&&(!presentation||preview_pad.focused)&&(!launch->frames||qa_controller||qa_town||opening_qa||qa_stick_combat||arena_qa) && !launch->benchmark && !ImGui::GetIO().WantCaptureKeyboard && !show_map && !show_graphics && !destination_loading &&
            terrain.view_ready(cam.x, cam.z, cam.fly);
        // Sample even while menus/loading own input so a held stick cannot
        // become a delayed swing when ordinary combat controls resume.
        const bool sword_pad_context=player_controls_allowed&&pad_playable&&preview_pad.get()&&fight.engaged()&&!arena_bow()&&!arena_look_held();
        const bool sword_pad_enabled=sword_pad_context&&preview_pad.using_pad;
        const bool sword_guard_held=preview_pad.left_trigger>.4f;
        // A centered connected controller can arm while keyboard/mouse owns
        // input. Its first deliberate stick movement then works immediately;
        // an inactive deflected stick remains disarmed and cannot replay later.
        const bool sword_stick_neutral=std::hypot(preview_pad.look.x,preview_pad.look.y)<=world_controller::SwordStick::neutral_radius;
        const auto sword_gesture=sword_stick.sample(preview_pad.look,sword_pad_context&&(preview_pad.using_pad||sword_stick_neutral),sword_guard_held,fight.state==TrainingState::Fighting);
        if (player_controls_allowed) {
            const Uint8 *keys = SDL_GetKeyboardState(nullptr);
            const float pad_forward=pad_playable?-preview_pad.move.y:0;
            const float pad_side=pad_playable?preview_pad.move.x:0;
            if(pad_playable){
                const float px=preview_pad.look.x,py=preview_pad.look.y;
                if(fight.engaged()&&!arena_bow()){
                    if(arena_look_held()){
                        cam.yaw+=px*dt*2.2f*preview_settings.sensitivity;
                        cam.pitch=std::clamp(cam.pitch-py*dt*1.5f*preview_settings.sensitivity*(preview_settings.invert_y?-1.f:1.f),-1.1f,1.1f);
                        fight.set_guard(false);fight.clear_buffer();
                    }else if(sword_pad_enabled){
                        if(sword_gesture.select_direction)fight.choose(sword_gesture.direction);
                        fight.set_guard(preview_pad.guard);
                        // LT wins even when buttons/RT arrive in the same SDL
                        // frame, and while a held trigger awaits neutral rearm.
                        if(!sword_guard_held){
                            if(pad_thrust_requested){fight.choose(TrainingDirection::Thrust);fight.attack();}
                            else if(sword_gesture.attack||preview_pad.attack_pressed||pad_attack_requested)fight.attack();
                        }
                    }
                }else{cam.yaw+=px*dt*2.2f*preview_settings.sensitivity;cam.pitch=std::clamp(cam.pitch-py*dt*1.5f*preview_settings.sensitivity*(preview_settings.invert_y?-1.f:1.f),-1.45f,1.45f);}
            }
            if(arena_enabled&&arena_target_lock.locked()&&!arena_bow()&&!arena_look_held())
                cam.yaw=arena_target_lock.steer(arena_game,{cam.x-arena_origin_x,cam.y-1.75-arena_floor,cam.z-arena_origin_z},cam.yaw,dt);
            if(fight.engaged()&&!look_active&&(!presentation||!preview_pad.using_pad)){look_active=true;SDL_SetRelativeMouseMode(SDL_TRUE);}
            const bool sprinting=keys[SDL_SCANCODE_LSHIFT]||(pad_playable&&preview_pad.sprint);
            float speed = cam.fly ? (studio.open?studio.fly_speed*(keys[SDL_SCANCODE_LSHIFT]?4.f:1.f):(keys[SDL_SCANCODE_LSHIFT] ? 700.f : 90.f)) : float(world_atlas_walk_speed()) * (sprinting ? 1.7f : 1.f);
            if(presentation&&!opening_enabled&&sprinting&&studio.components_runtime&&studio.components_runtime->counter(world_runtime_demo::renown_counter)>0)speed*=1.15f;
            if(fight.engaged())speed=arena_enabled?2.8f:2.35f;
            float forward = float(keys[SDL_SCANCODE_W]) - float(keys[SDL_SCANCODE_S])+pad_forward, side = float(keys[SDL_SCANCODE_D]) - float(keys[SDL_SCANCODE_A])+pad_side;
            float len = std::hypot(forward, side);
            if (len > 1) {
                forward /= len;
                side /= len;
            }
            V3 f = cam.fly ? cam.forward() : V3{std::sin(cam.yaw), 0, -std::cos(cam.yaw)}, r = cam.right();
            double nx = std::clamp(cam.x + (f.x * forward + r.x * side) * speed * dt, -world_atlas_width() * .5 + 1, world_atlas_width() * .5 - 1), nz = std::clamp(cam.z + (f.z * forward + r.z * side) * speed * dt,
                -world_atlas_depth() * .5 + 1, world_atlas_depth() * .5 - 1);
            if(fight.engaged()){
                double ox,oz;world_atlas_village_origin(&ox,&oz);
                if(presentation){ox=preview_arena_x;oz=preview_arena_z;}
                if(fight.dodge_time>0){const double impulse=4.*std::sin(3.14159265358979323846*fight.dodge_time/.26);nx-=f.x*impulse*dt;nz-=f.z*impulse*dt;}
                if(playtest_enabled){if(!playtest_combat_can_step(nx,nz)){nx=cam.x;nz=cam.z;}}
                else if(first_journey_enabled){if(!first_journey_can_step(nx,nz)){nx=cam.x;nz=cam.z;}}
                else{nx=std::clamp(nx,ox-52.,ox-44.);nz=std::clamp(nz,oz-77.,oz-69.);}
                const double dx=nx-ox-fight.enemy_x,dz=nz-oz-fight.enemy_z,sep=std::hypot(dx,dz);
                if(!arena_enabled&&sep<1.35){if(sep>.00001){nx=ox+fight.enemy_x+dx/sep*1.35;nz=oz+fight.enemy_z+dz/sep*1.35;}else{nx=cam.x;nz=cam.z;}}
            }
            if(arena_enabled){
                auto limited=arena_game.bounds.constrain_player({nx-arena_origin_x,0,nz-arena_origin_z});nx=arena_origin_x+limited.x;nz=arena_origin_z+limited.z;
                for(const auto &actor:arena_game.challengers()){
                    if(actor.state==world_arena_combat::ChallengerState::Unused||actor.state==world_arena_combat::ChallengerState::Defeated)continue;
                    const double radius=actor.state==world_arena_combat::ChallengerState::Active?1.35:.8;
                    const double dx=nx-arena_origin_x-actor.position.x,dz=nz-arena_origin_z-actor.position.z,d=std::hypot(dx,dz);
                    if(d<radius){if(d>.00001){nx=arena_origin_x+actor.position.x+dx/d*radius;nz=arena_origin_z+actor.position.z+dz/d*radius;}else{nx=cam.x;nz=cam.z;}}
                }
                limited=arena_game.bounds.constrain_player({nx-arena_origin_x,0,nz-arena_origin_z});nx=arena_origin_x+limited.x;nz=arena_origin_z+limited.z;
            }
            auto moved=move_camera(cam,scenery,nx,nz);
            manual_collision_steps+=moved.collision;manual_slope_steps+=moved.slope;manual_sliding_steps+=moved.sliding;
            if (cam.fly) {
                const float pad_height=source_showcase&&pad_playable?preview_pad.right_trigger-preview_pad.left_trigger:0.f;
                cam.y += (float(keys[SDL_SCANCODE_E]) - float(keys[SDL_SCANCODE_Q])+pad_height) * speed * dt;
                cam.y = std::max(cam.y, double(walking_height(cam.x, cam.z, nullptr) + 1.75f));
            } else {
                float floor = walking_height(cam.x, cam.z, nullptr) + 1.75f;
                if (!fight.engaged()&&(opening_enabled?opening_jump:bool(keys[SDL_SCANCODE_SPACE])) && (cam.grounded||cam.y <= floor + .02))
                    cam.vy = 4.7f;
                cam.vy -= 9.81f * dt;
                move_camera_vertical(cam,scenery,cam.y+cam.vy*dt,floor);
            }
        }

        opening_jump=false;
        if (launch->tour && frame > 1 && frame % 360 == 0) {
            current = (current + 1) % int(landmark_count);
            teleport(cam, landmarks[current]);
            current_name=landmarks[current].name;destination_loading=true;
            temporal_cut=true;
            benchmark_x = cam.x;
            benchmark_z = cam.z;
        }

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
        visual_grass_interactor={float(cam.x),float(cam.z),float(cam.y-1.75),visual_surface_finish&&!cam.fly&&!destination_loading?.95f:0.f};
        #include "world_runtime_service.inc"
        if(active_playtest_walk)playtest_walk_reference=cam.y-1.75;
        arena_controls(player_controls_allowed&&!arena_finished());
        if(arena_enabled){const bool aim=arena_bow()&&(arena_mouse_aim||(preview_pad.using_pad&&preview_pad.left_trigger>.4f));const float wanted=aim?.78f:1.08f;cam.fov+=(wanted-cam.fov)*(1-std::exp(-10.f*std::min(dt,.05f)));}
        preview_tick(dt);
        world_village_population_update(population,cam.x,cam.y,cam.z,visual_time,qa_fixed_time?0.f:(launch->frames?1.f/60.f:dt));
        population_peak_visible=std::max(population_peak_visible,world_village_population_stats(population).visible);
        const bool fight_was_engaged=fight.engaged();
        double fight_ox,fight_oz;world_atlas_village_origin(&fight_ox,&fight_oz);
        if(presentation){fight_ox=preview_arena_x;fight_oz=preview_arena_z;}
        const bool fight_paused=(arena_enabled&&!arena_controls_ready())||(first_journey_enabled?(preview_screen!=1||first_journey_dialogue_active):preview_blocked())||(studio.components_runtime&&studio.components_runtime->current_line())||studio.production_freeze()||studio.cinematic_preview||show_map||show_graphics||destination_loading||(!qa_training&&!qa_preview&&!qa_controller&&!playtest_combat_qa&&!qa_stick_combat&&!(SDL_GetWindowFlags(window)&SDL_WINDOW_INPUT_FOCUS));
        if(fight.engaged()&&fight_paused)fight_paused_frames++;
        fight.impact_pause_enabled=!presentation||!preview_settings.reduced_motion;
        const unsigned fight_old_swings=fight.player_swings,fight_old_enemy_swings=fight.enemy_swings,fight_old_guarded=fight.guarded;
        const unsigned fight_old_hits=fight.hits,fight_old_enemy_hits=fight.enemy_hits,fight_old_parries=fight.parries,fight_old_blocks=fight.blocks;
        const float fight_old_health=fight.health,fight_old_enemy_health=fight.enemy_health;
        first_journey_combat_step(launch->frames?1.f/60.f:dt,fight_paused);
        if(first_journey_enabled){fight_ox=preview_arena_x;fight_oz=preview_arena_z;}
        // The arena advances the one shared player clock and every active
        // opponent together. Ordinary one-on-one encounters retain this path.
        if(!arena_enabled)fight.update(launch->frames?1./60.:dt,cam.x-fight_ox,cam.z-fight_oz,fight_paused);
        arena_tick(launch->frames?1.f/60.f:dt,fight_paused);
        if(arena_enabled){fight_ox=preview_arena_x;fight_oz=preview_arena_z;}
        if(fight_paused)training_sounds.mute();
        if(!fight_paused){
            combat_contact_time=std::max(0.f,combat_contact_time-dt);
            if(fight.player_swings>fight_old_swings)training_sounds.play(0,42);
            else if(fight.enemy_swings>fight_old_enemy_swings)training_sounds.play(0,34);
            if(fight.parries>fight_old_parries)training_sounds.play(2,70);
            else if(fight.blocks>fight_old_blocks||fight.guarded>fight_old_guarded)training_sounds.play(2,55);
            if(fight.hits>fight_old_hits||fight.enemy_hits>fight_old_enemy_hits)training_sounds.play(1,68);
            if(fight.enemy_hits>fight_old_enemy_hits){combat_contact_kind=2;combat_contact_damage=std::max(0.f,fight_old_health-fight.health);combat_contact_time=.48f;}
            else if(fight.hits>fight_old_hits){combat_contact_kind=1;combat_contact_damage=std::max(0.f,fight_old_enemy_health-fight.enemy_health);combat_contact_time=.48f;}
            else if(fight.parries>fight_old_parries){combat_contact_kind=4;combat_contact_damage=0;combat_contact_time=.48f;}
            else if(fight.blocks>fight_old_blocks||fight.guarded>fight_old_guarded){combat_contact_kind=3;combat_contact_damage=0;combat_contact_time=.40f;}
            if(presentation){
                if(fight.enemy_hits>fight_old_enemy_hits){preview_hit=.35f;preview_pad.rumble(.65f,.3f,150,preview_settings.vibration);}
                else if(fight.parries>fight_old_parries){preview_parry=.3f;preview_pad.rumble(.2f,.75f,90,preview_settings.vibration);}
                else if(fight.hits>fight_old_hits)preview_pad.rumble(.25f,.4f,70,preview_settings.vibration);
            }
        }
        if(fight.state==TrainingState::Fighting&&!fight_paused){const unsigned arena=fight.trail_threat?1:0;if(++combat_timing_warmup[arena]>120){if(!combat_timing_windows[arena][0])combat_timing_windows[arena][0]=frame;combat_timing_windows[arena][1]=frame;}}
        if(!arena_enabled&&fight.engaged()&&!fight_paused){cam.yaw=float(std::atan2(fight_ox+fight.enemy_x-cam.x,-(fight_oz+fight.enemy_z-cam.z)));cam.pitch=-.12f;}
        if(fight_was_engaged&&!fight.engaged()){fight_end_frame=frame;look_active=false;SDL_SetRelativeMouseMode(SDL_FALSE);}
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
            staged_quality.reset({graphics_pending.auto_quality,graphics_pending.render_scale,graphics_pending.min_render_scale,float(graphics_pending.target_fps)},quality_epoch+1);
            const float staged_scale=graphics_pending.auto_quality?staged_quality.scale():graphics_pending.render_scale;
            const int rw=std::max(1,int(std::lround(scene_display_w*staged_scale))),rh=std::max(1,int(std::lround(scene_display_h*staged_scale)));
            Target staged_target;staged_target.separate_indirect=target.separate_indirect;
            Shadow staged_shadow;
            const bool change_target=rw!=target.w||rh!=target.h;
            const bool change_shadow=graphics_pending.shadow_side!=shadow.side;
            const bool allocated=display_ok&&(!change_target||staged_target.resize(rw,rh))&&(!change_shadow||staged_shadow.resize(graphics_pending.shadow_side))&&(!blood_renderer||!change_target||world_blood_prepare(blood_renderer,rw,rh));
            bool committed=false;
            if(!allocated){
                quality_apply_failures++;
                std::snprintf(graphics_message,sizeof(graphics_message),"Could not allocate this quality level. Previous settings retained.");
            }else if(world_graphics_settings_save(graphics_settings_path,graphics_pending,graphics_message,sizeof(graphics_message))){
                committed=true;
                if(change_target){target.release();target=staged_target;staged_target={};quality_reallocations++;}
                if(change_shadow){glDeleteFramebuffers(1,&shadow.fbo);glDeleteTextures(1,&shadow.depth);shadow.fbo=staged_shadow.fbo;shadow.depth=staged_shadow.depth;shadow.side=staged_shadow.side;staged_shadow.fbo=staged_shadow.depth=0;}
                graphics_apply_count++;graphics=graphics_pending;rendered_scale=staged_scale;quality=staged_quality;quality_epoch++;
                latest_color=target.color;temporal_cut=true;quality_valid_after=frame+60;display_resize=false;
                minimum_rendered_scale=std::min(minimum_rendered_scale,rendered_scale);maximum_rendered_scale=std::max(maximum_rendered_scale,rendered_scale);
                if(!graphics.reflections)reflection.release();
                std::printf("[Graphics] Applied %s, scene%dx%d shadow%d reflection%.2f enabled%d contact%d fog%d clouds%d\n",
                    world_graphics_preset_name(graphics.preset),target.w,target.h,shadow.side,graphics.reflection_scale,int(graphics.reflections),int(graphics.contact_shadows),int(graphics.volumetric_fog),int(graphics.clouds));
                std::printf("[Graphics] Display %s %dx%d VSync %d\n",graphics.display_mode==WorldDisplayMode::Borderless?"Borderless":"Windowed",display_w,display_h,SDL_GL_GetSwapInterval());
                std::snprintf(graphics_message,sizeof(graphics_message),"Applied and saved.");
            }
            if(!committed){
                if(blood_renderer&&!world_blood_prepare(blood_renderer,target.w,target.h)){std::fprintf(stderr,"[Blood] Could not restore resolution resources\n");launch->exit_code=1;running=false;}
                graphics_display(window,previous_display);SDL_GL_GetDrawableSize(window,&display_w,&display_h);SDL_GetWindowSize(window,&logical_w,&logical_h);refresh_designer_view();
            }
            actual_swap_interval=SDL_GL_GetSwapInterval();
            complete_before_swap=actual_swap_interval>0&&!presentation&&!(launch->frames&&qa_disable_completion&&!std::strcmp(qa_disable_completion,"1"));
            staged_target.release();staged_shadow.release();
        }
        const float requested_scale=graphics.auto_quality?quality.scale():graphics.render_scale;
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
                display_resize=false;quality_apply_failures++;graphics.auto_quality=false;graphics.render_scale=rendered_scale;
                graphics.min_render_scale=std::min(graphics.min_render_scale,rendered_scale);graphics.preset=WorldGraphicsPreset::Custom;graphics_draft=graphics;
                quality.reset({false,rendered_scale,graphics.min_render_scale,float(graphics.target_fps)},++quality_epoch);
                std::snprintf(graphics_message,sizeof(graphics_message),"Image resize unavailable. Automatic quality paused; previous resolution retained.");
            }
        }
        cam.aspect=world_designer_viewport::aspect(designer_view);
        studio.cinematic_camera(cam,SDL_GetTicks64()*.001);if(studio.cinematic_cut){temporal_cut=true;studio.cinematic_cut=false;}
        terrain.update(cam.x, cam.z, 2);
        Camera render_cam=cam;
        float follow_distance=0;
        const auto *preview_line=presentation&&studio.components_runtime?studio.components_runtime->current_line():nullptr;
        const bool opening_camera_locked=opening_enabled&&(opening_frame.cinematic||opening_frame.camp_dialogue||opening_frame.stage==world_opening_prologue::Stage::Title||opening_frame.stage==world_opening_prologue::Stage::GameMenu);
        if(opening_camera_locked){opening_render_camera(render_cam);dialogue_camera_active=false;}
        else if(preview_line&&!opening_enabled){
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
            const auto follow=world_follow_camera(cam.x,cam.y,cam.z,cam.yaw,cam.pitch,boom_distance,dt,temporal_cut,
                [&](double x,double z){return walking_height(x,z,nullptr);},
                [&](double ax,double az,double bx,double bz,float y,float h,float radius){
                    if(!world_scenery_can_move(scenery,ax,az,bx,bz,y,h,radius))return false;
                    if(playtest_enabled&&(active_playtest_walk||active_city_physics)){
                        world_physics::Hit hit;const world_physics::Vec3 center{ax,y+h*.5,az},delta{bx-ax,0,bz-az};
                        const float half=world_follow_camera_capsule_half_height(h,radius);
                        const bool queried=active_playtest_walk?active_playtest_walk->cast_capsule(center,radius,half,delta,hit):active_city_physics->cast_capsule(center,radius,half,delta,hit);
                        if(!queried||(hit.hit&&hit.fraction<.999f))return false;
                    }
                    return true;
                });
            render_cam.x=follow.x;render_cam.y=follow.y;render_cam.z=follow.z;follow_distance=boom_distance=follow.distance;
            if(presentation&&!arena_enabled&&fight.engaged()&&follow.distance>.75f){
                // Keep the opponent readable beside the player's silhouette.
                // Sweep the shoulder offset through the same camera clearance
                // contract as the boom; logical player/attack positions stay fixed.
                const auto side=cam.right();
                for(unsigned i=0;i<6;i++){
                    const double nx=render_cam.x+side.x*.175,nz=render_cam.z+side.z*.175;
                    if(walking_height(nx,nz,nullptr)+.22>render_cam.y||!world_scenery_can_move(scenery,render_cam.x,render_cam.z,nx,nz,float(render_cam.y-.18),.36f,.22f))break;
                    if(playtest_enabled&&(active_playtest_walk||active_city_physics)){
                        world_physics::Hit hit;const world_physics::Vec3 center{render_cam.x,render_cam.y,render_cam.z},delta{nx-render_cam.x,0,nz-render_cam.z};
                        const float half=world_follow_camera_capsule_half_height(.36f,.22f);
                        const bool queried=active_playtest_walk?active_playtest_walk->cast_capsule(center,.22f,half,delta,hit):active_city_physics->cast_capsule(center,.22f,half,delta,hit);
                        if(!queried||(hit.hit&&hit.fraction<.999f))break;
                    }
                    render_cam.x=nx;render_cam.z=nz;
                }
                const double dx=preview_arena_x+fight.enemy_x-render_cam.x,dz=preview_arena_z+fight.enemy_z-render_cam.z;
                render_cam.yaw=float(std::atan2(dx,-dz));render_cam.pitch=float(std::atan2(preview_arena_feet+1.35-render_cam.y,std::hypot(dx,dz)));
            }
        }else boom_distance=3.6f;
        portrait_focus_distance=0;
        if(milestone_review.portrait()) {
            const auto p=world_milestone::portrait(cam.x,cam.y-1.75,cam.z,cam.yaw,milestone_review);
            render_cam=cam;render_cam.x=p.x;render_cam.y=p.y;render_cam.z=p.z;render_cam.yaw=p.yaw;render_cam.pitch=p.pitch;render_cam.fov=p.fov;
            portrait_focus_distance=p.focus;follow_distance=p.focus;milestone_review.portrait_frames++;
        }
        arena_prepare_render(render_cam);
        dressing.update(render_cam,launch->frames?1./60.:double(dt));
        outcrops.update(render_cam);
        horizon.update(render_cam);
        const bool draw_avatar=third_person&&!cam.fly&&follow_distance>.75f&&(!presentation||preview_screen!=0);
        const double travel=avatar_initialized?std::hypot(cam.x-avatar_last_x,cam.z-avatar_last_z):0.;
        const bool avatar_walking=travel>.0005&&travel<1.;
        if(!avatar_initialized||travel>10)avatar_yaw=3.14159265359f-cam.yaw;
        else if(avatar_walking&&!fight.engaged()){
            const double desired=std::atan2(cam.x-avatar_last_x,cam.z-avatar_last_z);
            avatar_yaw+=float(std::remainder(desired-avatar_yaw,6.28318530718)*std::min(1.f,dt*14.f));
        }
        if(fight.engaged())avatar_yaw=3.14159265359f-cam.yaw;
        avatar_last_x=cam.x;avatar_last_z=cam.z;avatar_initialized=true;
        if(presentation)world_combat_motion_arena(combat_motion,preview_arena_x,preview_arena_z,preview_arena_feet);
        auto motion_fight=fight;
        if(arena_enabled)for(unsigned i=0;i<arena_game.events().count;++i)if(arena_game.events().values[i].kind==TrainingContactKind::EnemyBody)motion_fight.enemy_direction=arena_game.events().values[i].direction;
        if(arena_enabled)world_combat_motion_update_arena(combat_motion,motion_fight,cam.x,cam.y-1.75,cam.z,avatar_yaw,launch->frames?1.f/60.f:dt,arena_motion_actors.data(),unsigned(arena_motion_actors.size()),fight_paused);
        else world_combat_motion_update(combat_motion,fight,cam.x,cam.y-1.75,cam.z,avatar_yaw,launch->frames?1.f/60.f:dt,
            first_journey_enabled?first_journey_enemy_visible():arena_enabled?(arena_game.state()==world_arena_combat::State::Entering||arena_game.state()==world_arena_combat::State::Fighting||arena_game.state()==world_arena_combat::State::Lost):playtest_enabled?playtest_combat_visible():presentation?(fight.engaged()||fight_result_in_lane):(owned_enemy_near||fight.engaged()||fight_result_in_lane),fight_paused);
        {
            world_combat_vfx::Input input;input.first_person=!third_person;input.paused=fight_paused||studio.open;
            bool physical_blood_arrow=false;
            input.feet[0]={cam.x,cam.y-1.75,cam.z};input.feet[1]={fight_ox+fight.enemy_x,presentation?preview_arena_feet:24.,fight_oz+fight.enemy_z};
            const auto r=render_cam.right(),f=render_cam.forward(),u=norm(cross(r,f));
            for(unsigned actor=0;actor<2;++actor){WorldCombatBlade blade;if((actor||!arena_bow())&&world_combat_motion_blade(combat_motion,actor,blade)){input.blades[actor]={{blade.base[0],blade.base[1],blade.base[2]},{blade.tip[0],blade.tip[1],blade.tip[2]},true};}}
            if(input.first_person&&!arena_bow()){WorldCombatBlade blade;if(world_combat_motion_blade(combat_motion,0,blade,true)){const double lens=std::tan(render_cam.fov*.5)/std::tan(1.08*.5);auto point=[&](const double *p){return world_combat_vfx::Point{render_cam.x+r.x*p[0]*lens+u.x*p[1]*lens-f.x*p[2],render_cam.y+r.y*p[0]*lens+u.y*p[1]*lens-f.y*p[2],render_cam.z+r.z*p[0]*lens+u.z*p[1]*lens-f.z*p[2]};};input.trail_blades[0]={point(blade.base),point(blade.tip),true};}}
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
            input.attack_serial=fight.attack_start_serial;
            std::array<world_blood_effects::ResolvedEvent,64> arena_body_events={};unsigned arena_body_count=0;
            if(arena_enabled){
                input.resolved_events=true;
                input.actor_trails[0]={input.trail_blades[0].valid?input.trail_blades[0]:input.blades[0],
                    !arena_bow()&&fight.engaged()&&(fight.player_phase==TrainingPlayerPhase::Strike||(fight.player_phase==TrainingPlayerPhase::Windup&&fight.player_time>fight.player_duration*.62)),fight.attack_start_serial};
                for(unsigned id=0;id<arena_motion_actors.size();++id){
                    const auto &a=arena_motion_actors[id];const auto &v=a.combat;WorldCombatBlade blade;
                    if(a.visible&&world_combat_motion_blade(combat_motion,id+1,blade))input.actor_trails[id+1]={{{blade.base[0],blade.base[1],blade.base[2]},{blade.tip[0],blade.tip[1],blade.tip[2]},true},
                        v.engaged()&&(v.enemy_phase==TrainingEnemyPhase::Strike||(v.enemy_phase==TrainingEnemyPhase::Windup&&v.enemy_time>v.enemy_duration*.78)),v.enemy_swings};
                }
                const auto &events=arena_game.events();
                for(unsigned n=0;n<events.count;++n){
                    const auto &e=events.values[n];if(e.opponent_id>=arena_game.config().total)continue;
                    const bool outgoing=e.kind==TrainingContactKind::PlayerBody,incoming=e.kind==TrainingContactKind::EnemyBody;
                    const bool parry=e.kind==TrainingContactKind::PlayerParry,block=e.kind==TrainingContactKind::PlayerBlock,clash=e.kind==TrainingContactKind::EnemyBlock;
                    if(!outgoing&&!incoming&&!parry&&!block&&!clash)continue;
                    const auto &actor=arena_game.challengers()[e.opponent_id];
                    const world_combat_vfx::Point enemy_feet{arena_origin_x+actor.position.x,arena_floor,arena_origin_z+actor.position.z};
                    const auto defender=incoming?input.feet[0]:enemy_feet,attacker=incoming?enemy_feet:input.feet[0];
                    const double distance=std::max(.001,std::hypot(attacker.x-defender.x,attacker.z-defender.z));
                    const world_combat_vfx::Point outward{(attacker.x-defender.x)/distance,0,(attacker.z-defender.z)/distance},side{outward.z,0,-outward.x};
                    const double sign=e.direction==TrainingDirection::Left?1.:e.direction==TrainingDirection::Right?-1.:(e.serial&1)?1.:-1.;
                    world_combat_vfx::Point direction{outward.x*.35+side.x*.94*sign,e.direction==TrainingDirection::High?.16:.08,outward.z*.35+side.z*.94*sign};
                    world_combat_vfx::Point point{defender.x+outward.x*.34,defender.y+1.18,defender.z+outward.z*.34};
                    WorldCombatBlade enemy_blade;const bool has_enemy=world_combat_motion_blade(combat_motion,e.opponent_id+1,enemy_blade);
                    auto closest_point=[](world_combat_vfx::Point a,world_combat_vfx::Point b,world_combat_vfx::Point q){const double dx=b.x-a.x,dy=b.y-a.y,dz=b.z-a.z;const double t=std::clamp(((q.x-a.x)*dx+(q.y-a.y)*dy+(q.z-a.z)*dz)/std::max(1.e-8,dx*dx+dy*dy+dz*dz),0.,1.);return world_combat_vfx::Point{a.x+dx*t,a.y+dy*t,a.z+dz*t};};
                    if(outgoing||incoming){
                        world_combat_vfx::Blade weapon=input.blades[0];if(incoming&&has_enemy)weapon={{enemy_blade.base[0],enemy_blade.base[1],enemy_blade.base[2]},{enemy_blade.tip[0],enemy_blade.tip[1],enemy_blade.tip[2]},true};
                        if(weapon.valid){auto near=closest_point(weapon.base,weapon.tip,{defender.x,defender.y+1.25,defender.z});point.y=std::clamp(near.y,defender.y+.65,defender.y+1.48);}
                    }else if(has_enemy&&input.blades[0].valid){
                        const world_combat_vfx::Point a{enemy_blade.base[0],enemy_blade.base[1],enemy_blade.base[2]},b{enemy_blade.tip[0],enemy_blade.tip[1],enemy_blade.tip[2]};
                        const auto on_player=closest_point(input.blades[0].base,input.blades[0].tip,a),on_enemy=closest_point(a,b,on_player),again=closest_point(input.blades[0].base,input.blades[0].tip,on_enemy);
                        point={(again.x+on_enemy.x)*.5,(again.y+on_enemy.y)*.5,(again.z+on_enemy.z)*.5};
                    }else point={(attacker.x+defender.x)*.5,defender.y+1.25,(attacker.z+defender.z)*.5};
                    if(e.projectile){point={arena_origin_x+e.point[0],arena_floor+e.point[1],arena_origin_z+e.point[2]};direction={outward.x*.85+side.x*.5*sign,.08,outward.z*.85+side.z*.5*sign};}
                    const double length=std::max(.001,std::hypot(direction.x,direction.y,direction.z));direction={direction.x/length,direction.y/length,direction.z/length};
                    const bool finisher=e.finisher;
                    const float strength=finisher?1.5f:std::clamp(.85f+e.damage/90.f,.9f,1.25f);
                    input.contacts[input.contact_count++]={e.serial,outgoing?0u:incoming?3u:parry?1u:block?2u:4u,point,defender,direction,strength,finisher};
                    if(outgoing||incoming){
                        arena_body_events[arena_body_count++]={e.serial,incoming?0u:1u,{point,direction,arena_floor+.032,true,strength,finisher}};
                        combat_contact_kind=incoming?2:1;combat_contact_damage=e.damage;combat_contact_time=.48f;
                    }else{combat_contact_kind=parry?4:3;combat_contact_damage=0;combat_contact_time=.36f;}
                }
            }
            combat_vfx.update(fight,input,launch->frames?1./60.:dt);
            if(blood_renderer){
                const auto contacts=combat_vfx.stats();world_blood_effects::Input body;
                body.outgoing_hits=fight.hits;body.incoming_hits=fight.enemy_hits;body.combat_clock=fight.clock;body.paused=input.paused;
                body.resolved_events=arena_enabled;body.events=arena_body_events.data();body.event_count=arena_body_count;
                for(unsigned defender=0;defender<2;++defender){
                    // The dressed arena sand is .032m above its physics floor.
                    const auto point=contacts.last_body_contacts[defender];
                    const double ground=first_journey_enabled?std::max(world_atlas_walk_height(point.x,point.z),0.f):arena_floor+.032;
                    body.contacts[defender]={point,input.body_directions[defender],ground,true,input.body_strength[defender],input.body_finisher[defender]};
                }
                if(!blood_effects.update(body,launch->frames?1./60.:dt)){std::fprintf(stderr,"[Blood] Invalid body event input\n");launch->exit_code=1;running=false;}
            }
            if(presentation&&!preview_settings.reduced_motion&&!fight_paused){const auto impulse=combat_vfx.camera_impulse();render_cam.pitch+=impulse;render_cam.yaw+=impulse*.35f;}
        }
        auto draw_player=[&](unsigned program,const Camera &eye,bool shadow,bool show_player=true){
            if(!first_journey_enabled||(first_journey_outdoors()&&first_journey_state.weapon_unlocked()))world_combat_motion_draw(combat_motion,program,eye.x,eye.y,eye.z,shadow,show_player&&!cam.fly&&!arena_bow());
            if(arena_enabled){world_arena_render_draw(arena_renderer,program,eye.x,eye.y,eye.z,shadow);world_combat_motion_draw_crowd(combat_motion,program,eye.x,eye.y,eye.z,shadow);world_arena_render_draw_arrows(arena_renderer,program,eye.x,eye.y,eye.z,shadow);if(arena_bow()&&third_person){world_arena_render_draw_bow_actor(arena_renderer,program,eye.x,eye.y,eye.z,false,shadow);world_arena_render_draw_bow(arena_renderer,program,eye.x,eye.y,eye.z,shadow);}}
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
        world_scenery_update(scenery,render_cam.x,render_cam.y,render_cam.z,render_cam.yaw,render_cam.pitch,render_cam.aspect,std::tan(render_cam.fov*.5f));
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
            const int wanted=city_showcase?-1:destination_loading?world_wildlife_loading_desired(wildlife,cam.x,cam.z):
                world_wildlife_desired(wildlife,cam.x,cam.z);
            if(wanted>=-1&&wanted!=world_wildlife_stats(wildlife).habitat) {
                wildlife_pending=wanted;destination_loading=true;wildlife_loading_frame=true;
            }
        }
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
        if(arrival_camp_enabled)world_trail_camp_focus(arrival_camp,opening_frame.camp_dialogue?opening_frame.camp_actor:-1,
            cam.x,cam.z,qa_fixed_time?0.f:(launch->frames?1.f/60.f:std::clamp(dt,0.f,.1f)));
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
        M4 view = look({0, 0, 0}, forward, {0, 1, 0});
        M4 projection=perspective(render_cam.fov,render_cam.aspect,view_near_plane(),15000.f);
        float jitter[2]={};
        if(temporal) {
            world_temporal_jitter(temporal_sample,jitter);
            projection.m[8]-=2*jitter[0]/target.w;
            projection.m[9]-=2*jitter[1]/target.h;
        }
        M4 vp=product(projection,view);
        const float rainforest_weight=world_atlas_rainforest_weight(render_cam.x,render_cam.z);
        const auto rainforest_atmosphere=world_rainforest_atmosphere(cloud,rainforest_weight,visual_surface_finish);
        Lighting light = local_lighting(hour,cloud,visual_time,rainforest_weight,bearing,sun_strength,sky_fill);
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
            world_opening_interior::contains(opening_interior,{render_cam.x,render_cam.y,render_cam.z}):
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
        last_rain=(stronghold_finish?.40f:rain)*weather_weight;
        last_fog=graphics.volumetric_fog?std::clamp(mist*std::max(
            weather_disabled?0.f:rainforest_atmosphere.mist_weight,village_mist),0.f,1.5f):0.f;
        last_wind_x=1.1f*wind*weather_weight;last_wind_z=.38f*wind*weather_weight;
        const float wetness=stronghold_finish?0.f:std::clamp((.20f+.64f*rain)*weather_weight,0.f,1.f);
        WorldWeatherFrame weather_view=weather_frame(render_cam,light,right,up,forward,lightVP,target.w,target.h,
            2*jitter[0]/target.w,2*jitter[1]/target.h,last_rain,last_fog,last_wind_x,last_wind_z);
        weather_view.shadow=shadow.depth;weather_view.macro_height=macro_height;
        weather_view.cover_height=LandscapeMaterials::cover_height;weather_view.cover_userdata=&landscape;
        world_weather_update(weather,weather_view);
        const auto cinematic_audio_flags=SDL_GetWindowFlags(window);
        studio.cinematic_audio_frame(render_cam,SDL_GetTicks64()*.001,!destination_loading&&!(cinematic_audio_flags&SDL_WINDOW_MINIMIZED)&&(launch->frames||launch->benchmark||(cinematic_audio_flags&SDL_WINDOW_INPUT_FOCUS)));
        const bool opening_apartment_audio=opening_enabled&&(opening_state.stage()<=world_opening_prologue::Stage::Transport||opening_state.stage()==world_opening_prologue::Stage::GameMenu);
        const float opening_score_gain=opening_apartment_audio?(!opening_state.config().house_shots.empty()&&opening_state.stage()!=world_opening_prologue::Stage::Transport?.10f:.35f):1.f;
        if(presentation)preview_audio.update(dt,fight.engaged(),!destination_loading&&(launch->frames||(cinematic_audio_flags&SDL_WINDOW_INPUT_FOCUS)),preview_settings.music*opening_score_gain);
        if(presentation){const auto *line=studio.components_runtime?studio.components_runtime->current_line():nullptr;preview_voices.update(line?world_presentation::voice_line(line->id,preview_stage()):0,!destination_loading&&(launch->frames||(cinematic_audio_flags&SDL_WINDOW_INPUT_FOCUS)),preview_screen!=1,preview_settings.voice);}
        if(demo_audio) {
            const auto audio_window_flags=SDL_GetWindowFlags(window);
            const auto *audio_keys=SDL_GetKeyboardState(nullptr);
            WorldDemoAudioFrame audio_frame;
            audio_frame.x=cam.x;audio_frame.y=cam.y;audio_frame.z=cam.z;audio_frame.yaw=cam.yaw;
            audio_frame.dt=dt;audio_frame.rain=stronghold_finish?0.f:last_rain;audio_frame.wind=wind;audio_frame.hour=hour;
            audio_frame.grounded=cam.grounded&&!cam.fly&&(!stronghold_finish||cam.z<stronghold_z+14);
            audio_frame.running=audio_keys[SDL_SCANCODE_LSHIFT]!=0||(pad_playable&&preview_pad.sprint);
            audio_frame.focused=!destination_loading&&!(audio_window_flags&SDL_WINDOW_MINIMIZED)&&
                (launch->frames||launch->benchmark||(audio_window_flags&SDL_WINDOW_INPUT_FOCUS));
            audio_frame.quiet_interior=opening_enabled&&(opening_state.stage()<=world_opening_prologue::Stage::Transport||opening_state.stage()==world_opening_prologue::Stage::GameMenu);
            audio_frame.interior=audio_frame.quiet_interior||inside_tavern_shell(cam)||(town_enabled&&cam.x>-1291.5&&cam.x<-1276&&cam.z>-10296.5&&cam.z<-10277.5&&cam.y>24.1&&cam.y<28.5);
            audio_frame.covered=audio_frame.interior||LandscapeMaterials::cover_height(cam.x,cam.z,&landscape)>cam.y+.4;
            world_demo_audio_set_ambience_gain(demo_audio,studio.cinematic_outside());
            if(!world_demo_audio_update(demo_audio,audio_frame))demo_audio_update_failures++;
        }
        g_engine_perf.raw_update_ms=millis(frame_start);
        g_engine_perf.update_ms=g_engine_perf.raw_update_ms;
        render_start=SDL_GetPerformanceCounter();
        if (profile_gpu)queries.begin(frame);

        bool view_ready = terrain.view_ready(render_cam.x, render_cam.z, render_cam.fly);
        if(destination_loading) {
            WorldCoverMetrics staging={};world_cover_metrics(dressing.cover,&staging);
            if(view_ready&&world_scenery_streaming_stats(scenery).critical_pending==0&&!horizon.building&&!staging.near_building&&!staging.tree_building&&!wildlife_loading_frame&&!bandit_loading_frame&&!trail_camp_loading_frame&&!arrival_camp_loading_frame&&!dragons_loading_frame) {
                if(!ensure_local_probes(render_cam)){launch->exit_code=1;running=false;}
                if(running&&!warm_main_materials(render_cam,visual_time,false)){launch->exit_code=1;running=false;}
                if(running)destination_loading=false;
            }
            else view_ready=false;
        }
        if(!running)break;
        if (!view_ready) { temporal_cut=true;loading_frames++; longest_loading_streak = std::max(longest_loading_streak, ++loading_streak); }
        else loading_streak = 0;
        bool reflection_rendered=false;
        if(!view_ready||temporal_cut)quality_valid_after=std::max(quality_valid_after,frame+30);
        const auto window_flags=SDL_GetWindowFlags(window);
        const bool quality_eligible=view_ready&&frame>quality_valid_after&&!show_graphics&&
            !(window_flags&SDL_WINDOW_MINIMIZED)&&(launch->frames||(window_flags&SDL_WINDOW_INPUT_FOCUS));
        if(graphics.auto_quality)adaptive_queries.begin(frame,quality_epoch,0.,quality_eligible);
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
            if(!arena_enabled)world_scenery_draw(scenery,foliageShadow.id,render_cam.x,render_cam.y,render_cam.z,true);
            if(world_village_population_stats(population).visible){
                auto &p=live_programs.crowd_shadow;p.use();p.m4("uVP",lightVP);p.f("uTime",visual_time);weather_program(p,last_wind_x,last_wind_z,wetness);
                WorldModelDepthMaterialScope character_depth(p.id);world_village_population_draw(population,p.id,render_cam.x,render_cam.y,render_cam.z,true);foliageShadow.use();
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
            world_castle_dragons_draw(dragons,foliageShadow.id,render_cam.x,render_cam.y,render_cam.z,render_cam.yaw,render_cam.pitch,render_cam.aspect,2);
            if(!draw_live_shadow(render_cam,lightVP,visual_time)){launch->exit_code=1;running=false;}
            }
            glEnable(GL_CULL_FACE);
            glDisable(GL_POLYGON_OFFSET_FILL);
            if (profile_gpu)shadow_queries.end();
            g_engine_perf.shadow_gpu_ms=shadow_queries.milliseconds;
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
            const float water_level=draw_pool?float(pool->surface_y):draw_creek?float(WORLD_CASTLE_CREEK_SURFACE_Y):0.f;
            bool reflect_water=graphics.reflections&&(draw_creek||draw_pool||(render_cam.y<650.&&local_sample.shore_distance<2400.&&render_cam.pitch<.65f));
            // The raised134m image has exactly one consumer: the pool. Do not
            // alter draw_pool, owner selection or the opaque snapshot path.
            // Loading warm passes call draw_water_reflection directly and
            // intentionally continue exercising all four startup directions.
            if(reflect_water&&draw_pool&&water_level==float(pool->surface_y)&&
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
            M4 reflection_vp=vp;
            if (profile_gpu) reflection_queries.begin(frame);
            const int reflection_width=std::max(1,int(std::lround(target.w*graphics.reflection_scale)));
            const int reflection_height=std::max(1,int(std::lround(target.h*graphics.reflection_scale)));
            const bool reflection_needs_allocation=reflection.w!=reflection_width||reflection.h!=reflection_height||!reflection.fbo;
            if(reflect_water&&reflection.resize(reflection_width,reflection_height)) {
                if(!first_reflection_frame)first_reflection_frame=frame;
                if(reflection_needs_allocation)reflection_play_allocations++;
                reflection_rendered=true;
                if(!draw_water_reflection(render_cam,light,torch_lights,lightVP,visual_time,water_level,
                        last_wind_x,last_wind_z,wetness,qa_fixed_time?0.f:std::clamp(visual_time-previous_rendered_visual_time,0.f,.1f),
                        reflection_vp,true,frame)) {launch->exit_code=1;running=false;}

            } else reflect_water=false;
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
                landscape.bind_shelter(program);studio_surface_gpu.bind(program,cam.x,cam.y,cam.z);
                program.i("uClipBelowWater",0);program.f("uWaterClipLevel",0);
                program.m4("uVP",vp);program.m4("uLightVP",lightVP);
                light_program(program,light,render_cam);torch_program(program,torch_lights);
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
            };
            auto prepare_main_foliage=[&](){prepare_main_foliage_program(foliage);};
            const bool early_opaque=inside_tavern_shell(render_cam);
            // Authored forest rocks, trunks and understory can occlude large
            // areas of terrain/procedural grass just like village buildings.
            // Submit them once before that work, with identical materials and
            // targets; retain the same distance-selected shadow caster set.
            const bool authored_first=authored_first_enabled&&
                (stronghold_finish||world_atlas_village_weight(render_cam.x,render_cam.z)>0.f||world_atlas_rainforest_weight(render_cam.x,render_cam.z)>0.f);
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
                foliage.i("uSourceMaterialAO",city_showcase?1:0);
                if(!arena_enabled)world_scenery_draw_filtered(scenery,foliage.id,render_cam.x,render_cam.y,render_cam.z,false,
                    authored_first?nullptr:"village_tavern",nullptr);
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
            landscape.bind(land);studio_surface_gpu.bind(land,cam.x,cam.y,cam.z);studio.surface_terrain_binding(land);
            world_snow_bind_terrain(snow,land.id);
            showcase_terrain.bind(land.id);
            if (profile_gpu)terrain_queries.begin(frame);
            terrain.draw(land, render_cam, vp);
            if (profile_gpu)terrain_queries.end();
            // Snapshot opaque attachments before water writes the main target.
            // Inland/ridge views skip this bandwidth and lazy allocation.
            if (profile_gpu)water_queries.begin(frame);
            draw_water_layers(render_cam,light,torch_lights,lightVP,vp,reflection_vp,visual_time,water_level,
                last_wind_x,last_wind_z,wetness,last_rain,jitter,reflect_water,draw_pool,draw_creek,true);
            if (profile_gpu)water_queries.end();
            if (profile_gpu)foliage_queries.begin(frame);
            prepare_main_foliage();
            dressing.draw(foliage, render_cam);
            outcrops.draw(foliage,render_cam);
            horizon.draw(foliage,render_cam);
            if(authored_first) {
                if(early_opaque)late_opaque_skipped_frames++;
            } else if(early_opaque) {
                foliage.i("uSourceMaterialAO",city_showcase?1:0);
                world_scenery_draw_filtered(scenery,foliage.id,render_cam.x,render_cam.y,render_cam.z,false,nullptr,"village_tavern");
                foliage.i("uSourceMaterialAO",0);
                late_opaque_skipped_frames++;
            } else {foliage.i("uSourceMaterialAO",city_showcase?1:0);if(!arena_enabled)world_scenery_draw(scenery,foliage.id,render_cam.x,render_cam.y,render_cam.z);foliage.i("uSourceMaterialAO",0);}
            if(world_village_population_stats(population).visible){prepare_main_foliage_program(live_programs.crowd);draw_population_color(live_programs.crowd,render_cam,vp);foliage.use();}
            world_castle_training_draw(training,foliage.id,render_cam.x,render_cam.y,render_cam.z);
            if(source_flames_enabled){torch_program(foliage,actor_practical_lights);foliage.i("uActorLighting",1);}
            draw_player(foliage.id,render_cam,false,draw_avatar);
            if(arena_bow()&&!third_person){Camera flat=render_cam;flat.pitch=0;foliage.m4("uVP",product(perspective(1.08f,render_cam.aspect,view_near_plane(),15000.f),look({0,0,0},flat.forward(),{0,1,0})));glDisable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);world_arena_render_draw_bow_actor(arena_renderer,foliage.id,render_cam.x,render_cam.y,render_cam.z,true,false);world_arena_render_draw_bow(arena_renderer,foliage.id,render_cam.x,render_cam.y,render_cam.z,false);glDepthMask(GL_TRUE);glEnable(GL_DEPTH_TEST);foliage.m4("uVP",vp);}
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
            world_castle_dragons_draw(dragons,foliage.id,render_cam.x,render_cam.y,render_cam.z,render_cam.yaw,render_cam.pitch,render_cam.aspect,std::tan(render_cam.fov*.5f));
            if(!draw_live_color(render_cam,vp,lightVP,light,torch_lights,visual_time,wetness,false,0)){launch->exit_code=1;running=false;}
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
            weather_view.color=latest_color;weather_view.depth=target.depth;
            if (profile_gpu)weather_queries.begin(frame);
            unsigned weather_color=world_weather_resolve(weather,weather_view);
            if((last_rain>0||last_fog>0)&&weather_color==latest_color) {
                weather_failures++;launch->exit_code=1;running=false;
            }
            latest_color=weather_color;
            latest_color=world_scene_particles_resolve(scene_particles,scene_particle_frame(render_cam,light,right,up,forward,
                vp,lightVP,target.w,target.h,latest_color,target.depth,shadow.depth));
            WorldCombatVfxFrame combat_frame;combat_frame.near_plane=view_near_plane();combat_frame.color=latest_color;combat_frame.depth=target.depth;combat_frame.width=target.w;combat_frame.height=target.h;std::copy_n(vp.m,16,combat_frame.vp);combat_frame.camera={render_cam.x,render_cam.y,render_cam.z};combat_frame.right[0]=right.x;combat_frame.right[1]=right.y;combat_frame.right[2]=right.z;combat_frame.up[0]=up.x;combat_frame.up[1]=up.y;combat_frame.up[2]=up.z;
            if(!world_combat_vfx_draw(combat_vfx_renderer,combat_vfx,combat_frame)){launch->exit_code=1;running=false;}
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
            if((fight.engaged()||((playtest_enabled||(first_journey_enabled&&first_journey_outdoors()&&first_journey_state.weapon_unlocked()))&&preview_screen==1&&!destination_loading&&!first_journey_dialogue_active))&&!third_person&&!cam.fly&&!arena_bow()){
                const V3 sun_camera={dot(light.sun,right),dot(light.sun,up),-dot(light.sun,forward)};
                const V3 world_up_camera={right.y,up.y,-forward.y};
                if(!training_weapon.draw(combat_motion,latest_color?latest_color:target.color,target.w,target.h,sun_camera,light.sunColor,light.skyLight,light.groundLight,world_up_camera)){
                    std::fprintf(stderr,"[World] First person arms target failed\n");launch->exit_code=1;running=false;
                }
            }
            if(temporal) {
                if (profile_gpu)temporal_queries.begin(frame);
                WorldTemporalFrame temporal_frame;temporal_frame.near_plane=view_near_plane();
                temporal_frame.color=latest_color;temporal_frame.depth=target.depth;
                temporal_frame.width=target.w;temporal_frame.height=target.h;
                std::memcpy(temporal_frame.view_projection,vp.m,sizeof(vp.m));
                temporal_frame.camera[0]=render_cam.x;temporal_frame.camera[1]=render_cam.y;temporal_frame.camera[2]=render_cam.z;
                temporal_frame.jitter_pixels[0]=jitter[0];temporal_frame.jitter_pixels[1]=jitter[1];
                temporal_frame.reset=temporal_cut||std::fabs(hour-history_hour)>.05f||std::fabs(cloud-history_cloud)>.04f||
                    std::fabs(rain-history_rain)>.05f||std::fabs(wind-history_wind)>.10f||std::fabs(mist-history_mist)>.08f||
                    std::fabs(bearing-history_bearing)>.10f||std::fabs(sun_strength-history_sun_strength)>.02f||std::fabs(sky_fill-history_sky_fill)>.02f;
                temporal_frame.reactive_alpha=true;
                world_temporal_resolve(temporal,temporal_frame);
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
        if (profile_gpu)post_queries.begin(frame);
        const GLuint scene=world_temporal_output(temporal,latest_color?latest_color:target.color);
        const GLuint glow=bloom_enabled?world_bloom_resolve(bloom,scene,target.w,target.h,exposure):0;
        if(bloom_enabled&&!glow){bloom_failures++;launch->exit_code=1;running=false;}
        if(glow)bloom_play_frames++;
        compose_post(scene,world_temporal_depth(temporal,target.depth),glow);
        if (profile_gpu)post_queries.end();
        glDepthMask(GL_TRUE);
        if(!launch->profile[0]) {
            foliage_sequence.capture(frame,launch->frames,display_w,display_h);
            npc_sequence.capture(frame,launch->frames,display_w,display_h);
        }
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
        if((presentation&&preview_menu_input)||studio.open||show_tools||show_graphics||show_map||ui_loading||(studio.components_runtime&&studio.components_runtime->current_line()))editor_ui_begin_frame(&ui);
        else editor_ui_begin_passive_frame(&ui,dt,display_w,display_h);
        if (ui_loading&&!presentation) {
            ImGui::SetNextWindowPos({float(display_w) * .5f, float(display_h) * .5f}, ImGuiCond_Always, {.5f, .5f});
            ImGui::Begin("Preparing destination", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove);
            ImGui::Text("Streaming %s...", current_name);
            ImGui::TextUnformatted("The previous view stays visible until the terrain is ready.");
            ImGui::End();
        }

        #include "engine/world_studio_runtime_qa.inc"
        #include "engine/world_studio_production_qa.inc"
        #include "engine/world_studio_designer_qa.inc"
        #include "engine/world_editor_copy_paste_qa.inc"
        #include "engine/world_editor_terrain_snap_qa.inc"
        #include "engine/world_editor_vfx_qa.inc"
        #include "engine/world_editor_completion_qa.inc"
        #include "engine/world_editor_optimization_qa.inc"
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
        const bool ui_was_flying=cam.fly;
        ImGui::BeginDisabled(launch->benchmark);
        if ((!studio.open&&show_tools)||show_map)
            destination = world_panel(cam, terrain, materials[7], atlas_map_attempted, show_tools&&!studio.open, show_map, show_graphics, hour, cloud, rain, wind, mist,
                bearing,sun_strength,sky_fill,exposure,grade_strength,fps,running,current_name,scenic_destination,population);
        ImGui::EndDisabled();
        studio.refresh_scene(scenery);studio.project_streaming=world_scenery_streaming_stats(scenery);studio.panel(render_cam,fps,terrain.bytes(),show_graphics);
        studio.production_service(!launch->frames&&!launch->benchmark&&!ui_loading&&!studio.drag.active&&!ImGui::IsAnyItemActive());
        if((studio.production_freeze()||studio.cinematic_preview)&&look_active){look_active=false;fight.set_guard(false);SDL_SetRelativeMouseMode(SDL_FALSE);}
        studio.components_dialogue();
        if(!presentation)draw_journey();
        if(cam.fly!=ui_was_flying)temporal_cut=true;
        if(graphics_panel(window,show_graphics,graphics_draft,graphics,quality,fps,display_w,display_h,target.w,target.h,graphics_message)) {
            if(world_graphics_settings_valid(graphics_draft)){
                graphics_pending=graphics_draft;graphics_apply=true;
                std::snprintf(graphics_message,sizeof(graphics_message),"Preparing selected quality...");
            }
        }
        double combat_ox,combat_oz;world_atlas_village_origin(&combat_ox,&combat_oz);
        if(!playtest_enabled&&!opening_enabled&&!launch->no_hud&&!studio.open){ImGui::SetNextWindowPos({float(display_w)-16,16},ImGuiCond_Always,{1,0});ImGui::SetNextWindowBgAlpha(.45f);ImGui::Begin("Camera mode",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoInputs|ImGuiWindowFlags_NoSavedSettings);ImGui::Text("%s | V: switch view",third_person?"Third person":"First person");ImGui::End();}
        if(!presentation&&!studio.open)training_panel(fight,fight.near(cam.x-combat_ox,cam.y,cam.z-combat_oz,cam.fly),!cam.fly&&std::hypot(cam.x-combat_ox-fight.instructor_x,cam.z-combat_oz-fight.instructor_z)<14.,fight_paused,display_w,display_h,!third_person);
        preview_ui(render_cam,ui_loading);
        editor_ui_render(&ui);
        if (destination >= 0&&!studio.production_freeze()&&!studio.cinematic_preview) {
            fight.leave();look_active=false;SDL_SetRelativeMouseMode(SDL_FALSE);
            current = destination;
            teleport(cam, landmarks[current]);
            current_name=landmarks[current].name;destination_loading=true;
            temporal_cut=true;
            benchmark_x = cam.x;
            benchmark_z = cam.z;
        }

        if (scenic_destination>=0&&!studio.production_freeze()&&!studio.cinematic_preview) {
            fight.leave();look_active=false;SDL_SetRelativeMouseMode(SDL_FALSE);
            scenic_travel(cam,unsigned(scenic_destination));
            current_name=scenic_views[scenic_destination].name;destination_loading=true;
            temporal_cut=true;
            benchmark_x=cam.x; benchmark_z=cam.z;
        }

        surface_workflow(unsigned(frame));
        live_workflow();
        if(studio.apply&&studio.try_fast_object_move(scenery)){
            studio_castle_culling=!studio.moved_architecture();temporal_cut=true;
        }
        if(studio.apply){
            // Revert render-only drag poses before validating committed
            // collision, grounding and shelter changes against the old scene.
            studio.clear_object_preview(scenery);
            studio.apply=false;std::string error;
            if(!studio.production_retry_components()){studio.production_restore_failed=true;studio.rejected++;}
            else if(!studio.production_sync(error)||!world_studio::valid(studio.draft,error)||!studio.components_scene_prevalidate(studio.draft,error)){studio.production_restore_failed=studio.production_restore;studio.rejected++;studio.message=error;}
            else{
                const bool rebuild=studio.rebuild||studio.object_lighting_pending||!world_surface::same_paths(studio.draft.surfaces,studio.live.surfaces);
                // Shutdown joins the worker before callbacks/documents change.
                const bool terrain_rebuild=studio.terrain_inputs_changed(studio.draft,studio.live);
                const auto apply_started=std::chrono::steady_clock::now();
                if(terrain_rebuild)terrain.release();
                auto next=studio.draft;const bool field_ready=!terrain_rebuild||studio.prepare_surface_field(next,error);if(!terrain_rebuild)next.surface_field=studio.live.surface_field;
                if(terrain_rebuild)world_atlas_set_height_edit(next.terrain.empty()&&!next.surface_field?nullptr:Studio::height,&next);
                if(terrain_rebuild)world_atlas_set_water_edit(next.waters.empty()&&!next.surface_field?nullptr:Studio::water_sample,&next);
                WorldScenery *next_scene=rebuild&&field_ready?studio.scene(next,error,scenery):nullptr;
                LandscapeMaterials next_landscape;WorldSurfaceGPU next_surfaces;bool prepared=field_ready&&(!rebuild||next_scene);
                const bool masks_changed=studio.object_lighting_pending||terrain_rebuild||(rebuild&&next_scene&&Studio::shelter_changed(scenery,next_scene));
                const bool surfaces_changed=masks_changed||Studio::surface_changed(next.surfaces,studio.live.surfaces);
                if(prepared&&masks_changed)prepared=next_landscape.init(rebuild?next_scene:scenery);
                if(prepared&&surfaces_changed)prepared=next_surfaces.prepare(masks_changed?next_landscape.masks:landscape.masks,next.surfaces,error);
                Studio::RuntimePrepared runtime_next;if(prepared)prepared=studio.prepare_runtime(next,rebuild?next_scene:scenery,runtime_next,error);
                const bool features=!next.materials.empty()||!next.lights.empty()||!next.decals.empty()||studio.preview_lod>=0;
                WorldEnvironment *next_environment=nullptr;
                const bool replace_environment=next.environment.enabled&&(!studio_environment||next.environment.path!=studio.live.environment.path);
                if(prepared&&replace_environment){next_environment=world_environment_create(next.environment.path.c_str(),error);prepared=next_environment!=nullptr;}
                if(prepared&&(studio.reload_shaders||(!next.surfaces.winds.empty())!=studio_wind_features||WorldSurfaceGPU::features(next.surfaces)!=studio_imported_features||features!=studio_shader_features||next.environment.enabled!=studio_environment_features||Studio::graphs_changed(next,studio.live)||next.waters.empty()!=studio.live.waters.empty())){
                    prepared=reload_studio_shaders(features,next.environment.enabled,&next,true);if(!prepared)error="Shader link failed. Previous programs and scene preserved.";
                }
                if(prepared&&rebuild&&!world_scenery_commit_prepared(next_scene,scenery)){prepared=false;error="Prepared scene owner changed; previous scene preserved.";}
                if(prepared)prepared=studio.commit_runtime(runtime_next,scene_particles,torch_flames);
                finish_programs(prepared);
                studio.reload_shaders=false;studio.fail_shader_test=false;
                if(prepared){
                    if(surfaces_changed){studio_surface_gpu.release();studio_surface_gpu=std::move(next_surfaces);}
                    if(replace_environment){world_environment_free(studio_environment);studio_environment=next_environment;}
                    if(!next.environment.enabled){world_environment_free(studio_environment);studio_environment=nullptr;}
                    if(terrain_rebuild){studio.live=std::move(next);studio.bind();}
                    else{
                        // Tile workers read only live.terrain. Scalar look and
                        // render-thread-only data can commit without changing
                        // that vector or rebuilding unchanged model buffers.
                        studio.live.placements=std::move(next.placements);studio.live.entities=std::move(next.entities);studio.live.source_signature=next.source_signature;studio.live.look=next.look;studio.live.lights=std::move(next.lights);studio.live.materials=std::move(next.materials);studio.live.decals=std::move(next.decals);studio.live.splines=std::move(next.splines);studio.live.groups=std::move(next.groups);studio.live.assets=std::move(next.assets);studio.live.environment=std::move(next.environment);studio.live.overrides=std::move(next.overrides);studio.live.actors=std::move(next.actors);studio.live.navigation=std::move(next.navigation);studio.live.particles=std::move(next.particles);studio.live.graphs=std::move(next.graphs);studio.live.surfaces=std::move(next.surfaces);
                    }
                    if(rebuild){world_scenery_free(scenery);scenery=next_scene;}if(masks_changed){landscape.release();landscape=std::move(next_landscape);}
                    if(terrain_rebuild){horizon.target_x=horizon.target_z=1e30;horizon.building=false;horizon.has_selection=false;}
                    if(terrain_rebuild){
                        // Match distant terrain shadow/weather lookup to the
                        // committed collision/mesh heights, including undo.
                        std::vector<float> heights(size_t(1025)*577);
                        for(int iz=0;iz<577;iz++)for(int ix=0;ix<1025;ix++)heights[size_t(iz)*1025+ix]=world_atlas_sample_uv(double(ix)/1024,double(iz)/576).height;
                        WorldVillageUnpackScope unpack;glBindTexture(GL_TEXTURE_2D,macro_height);glTexSubImage2D(GL_TEXTURE_2D,0,0,0,1025,577,GL_RED,GL_FLOAT,heights.data());
                    }
                    studio_castle_culling=!studio.moved_architecture();if(rebuild)refresh_tavern_shell();if(terrain_rebuild)world_weather_invalidate_surface(weather);
                    studio.materials(scenery);if(terrain_rebuild)world_cover_set_filter(dressing.cover,studio.live.stamps.empty()&&!studio.live.surface_field?nullptr:Studio::cover,&studio.live);
                    const auto &l=studio.live.look;hour=l.hour;cloud=l.cloud;rain=l.rain;wind=l.wind;mist=l.fog;bearing=l.bearing;sun_strength=l.sun;sky_fill=l.sky;exposure=l.exposure;
                    for(auto *&cache:local_probe_regions){world_local_probes_free(cache);cache=nullptr;}
                    studio.object_lighting_pending=false;studio.applied++;studio.dirty=studio.rebuild=false;studio.production_applied();const bool component_commit=studio.components_scene_applied();if(rebuild)live_visual_generation=0;if(component_commit)studio.message="Applied. Rebuilding streamed roots and local lighting; Save stores this world.";else{active_world_runtime=nullptr;studio.components_ready=false;studio.open=true;}
                    if(studio.target)studio.y=world_atlas_height(studio.x,studio.z);
                    if(!studio.open){cam.y=std::max(cam.y,double(world_atlas_walk_height(cam.x,cam.z))+1.75);}temporal_cut=true;destination_loading=true;
                }else{
                    if(replace_environment&&next_environment)world_environment_free(next_environment);
                    if(next_scene)world_scenery_free(next_scene);
                    next_surfaces.release();next_landscape.release();if(terrain_rebuild)studio.bind();if(rebuild){std::string ignored;studio.configure(studio.live,ignored);}
                    studio.production_restore_failed=studio.production_restore;studio.rejected++;studio.message=error.empty()?"Could not prepare lighting masks; previous scene preserved.":error;
                }
                studio.last_apply_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-apply_started).count();studio.last_apply_terrain=terrain_rebuild;studio.last_apply_masks=masks_changed;if(terrain_rebuild)studio.terrain_rebuild_count++;else studio.terrain_rebuild_skipped++;
                if(terrain_rebuild&&!terrain.init()){launch->exit_code=1;running=false;studio.message="Could not restart terrain streaming.";}
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
        grandstone_obstacle_capture();
        opening_prologue_qa_capture();
        first_journey_qa_capture();
        sword_stick_qa_capture();
        arena_qa_capture();
        engine_launch_frame(window);
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
        adaptive_queries.cadence(frame,millis(frame_start)*.001);
        if(source_review.active){
            source_review.record(frame,cam,source_review_waiting_frame||!view_ready||destination_loading,millis(frame_start),g_engine_perf.render_total_gpu_ms);
            if(source_review.complete)running=false;
        }
    }

    if(!source_review.finish()){std::fprintf(stderr,"[Source review] Failed to write report\n");launch->exit_code=1;}
    engine_perf_finish();
    // Opt-in inventory runs after the last measured frame, before resources
    // are destroyed. Ordinary play performs no additional GL queries.
    const std::string gpu_budget_path=std::string(launch->profile[0]?launch->profile:
        launch->capture[0]?launch->capture:"valwood")+".gpu-budget.json";
    const auto trail_camp_state=world_trail_camp_stats(trail_camp);
    const auto arrival_camp_state=world_trail_camp_stats(arrival_camp);
    const auto gpu_budget_audit=world_gpu_budget_audit(gpu_budget_path.c_str());
    if(gpu_budget_audit.requested&&(!gpu_budget_audit.available||!gpu_budget_audit.saved||!gpu_budget_audit.complete))
        launch->exit_code=1;
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
    const auto training_state=world_castle_training_stats(training);
    const auto watch_state=world_village_watch_stats(watch);
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
    const auto particle_state=world_scene_particles_stats(scene_particles);
    const bool training_audio_ready=training_sounds.ready;const unsigned training_audio_events=training_sounds.events;
    const unsigned training_body_audio=training_sounds.body_events,training_metal_audio=training_sounds.metal_events;
    studio.living_shutdown(!launch->frames&&!launch->benchmark);
    const auto demo_audio_state=world_demo_audio_stats(demo_audio);
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
    const auto combat_motion_state=world_combat_motion_stats(combat_motion);
    const auto combat_vfx_state=combat_vfx.stats();const auto combat_vfx_render_state=world_combat_vfx_stats(combat_vfx_renderer);
    const auto blood_state=blood_effects.stats();const auto blood_render_state=world_blood_stats(blood_renderer);
    const auto arena_render_state=world_arena_render_stats(arena_renderer);
    world_arena_render_free(arena_renderer);world_combat_vfx_free(combat_vfx_renderer);world_blood_free(blood_renderer);world_combat_motion_free(combat_motion);
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
    world_torch_flames_free(torch_flames);
    active_live_visuals=nullptr;active_world_runtime=nullptr;if(live_visuals)live_visuals->release();live_programs.free();previous_live.free();
    world_scenery_free(scenery);
    dressing.release();
    outcrops.release();
    horizon.release();
    const auto surface_paint_count=studio_surface_gpu.paint.tiles.size(),surface_river_count=studio_surface_gpu.rivers.ranges.size();const auto surface_gpu_bytes=studio_surface_gpu.bytes;
    studio_surface_gpu.release();
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
    world_temporal_free(temporal);world_sky_free(sky);world_occlusion_free(occlusion);world_weather_free(weather);
    world_bloom_free(bloom);world_scene_particles_free(scene_particles);
    studio.cinematic_audio_shutdown();
    const unsigned recorded_voice_lines=preview_voices.available,recorded_voice_playbacks=preview_voices.playbacks;
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
            std::fprintf(f,"  \"first_journey\": {\"enabled\": %s, \"stage\": %s, \"camp_mask\": %u, \"loaned\": %s, \"supplies\": %s, \"rewarded\": %s, \"complete\": %s, \"saves\": %u, \"restores\": %u, \"coins\": %u, \"food\": %u, \"reputation\": %u, \"live_wins\": %u, \"live_losses\": %u, \"enemy_steps\": %u, \"reward_health_before\": %.3f, \"reward_health_after\": %.3f},\n",first_journey_enabled?"true":"false",world_json::quote(world_first_journey::stage_name(first_journey_state.stage())).c_str(),first_journey_state.camp_spoken(),first_journey_state.weapon_unlocked()?"true":"false",first_journey_state.supplies_recovered()?"true":"false",first_journey_state.reward_granted()?"true":"false",first_journey_state.town_reached()?"true":"false",first_journey_saves,first_journey_restores,first_journey_frame.coins,first_journey_frame.food,first_journey_frame.reputation,first_journey_live_wins,first_journey_live_losses,first_journey_enemy_steps,first_journey_reward_health_before,first_journey_reward_health_after);
            std::fprintf(f,"  \"opening_interior\": {\"enabled\": %s, \"probe_domains\": %zu, \"release_city_fallbacks\": %s, \"city_suspended\": %s, \"city_groups\": %zu, \"policy_changes\": %llu},\n",
                opening_interior.enabled?"true":"false",opening_interior.probes.size(),opening_interior.release_city_fallbacks?"true":"false",
                live_stream_state.interior_city_suspended?"true":"false",live_stream_state.interior_city_groups,(unsigned long long)live_stream_state.interior_policy_changes);
            std::fprintf(f,"  \"playtest_combat\": {\"ready\": %s, \"starts\": %u, \"wins\": %u, \"defeats\": %u, \"hero_draw_frames\": %llu, \"enemy_visible_frames\": %llu, \"start_x\": %.6f, \"start_z\": %.6f, \"enemy_x\": %.6f, \"enemy_z\": %.6f, \"feet\": %.6f, \"error\": %s},\n",playtest_combat_ready?"true":"false",playtest_combat_starts,playtest_combat_wins,playtest_combat_defeats,(unsigned long long)playtest_hero_draw_frames,(unsigned long long)playtest_enemy_visible_frames,playtest_combat_start_x,playtest_combat_start_z,playtest_combat_enemy_x,playtest_combat_enemy_z,double(playtest_combat_feet),world_json::quote(playtest_combat_error).c_str());
            std::fprintf(f,"  \"view_clip\": {\"near_plane\": %.4f, \"far_plane\": 15000, \"imported_walk_geometry\": %s},\n",view_near_plane(),active_playtest_walk?"true":"false");
            std::fprintf(f,"  \"playtest_input\": {\"enabled\": %s, \"passed\": %s, \"phase\": %u, \"events\": %u, \"cases\": %u, \"synthetic_completion_for_travel_control_only\": %s},\n",playtest_input_qa?"true":"false",playtest_input_done?"true":"false",playtest_input_phase,playtest_input_events,playtest_input_cases,playtest_input_synthetic_completion?"true":"false");
            std::fprintf(f,"  \"controller\": {\"qa\": %s, \"qa_passed\": %s, \"phase\": %u, \"assertions\": %u, \"connects\": %u, \"disconnects\": %u, \"rumble_requests\": %u},\n",qa_controller?"true":"false",controller_qa_passed?"true":"false",controller_qa_phase,controller_qa_checks,preview_pad.connects,preview_pad.disconnects,preview_pad.rumble_requests);
            sword_stick_qa_report(f);
            arena_qa_report(f);
            std::fprintf(f,"  \"arena_render\": {\"draws\": %llu, \"bow_draws\": %llu, \"arrow_draws\": %llu, \"bow_pose_assets\": %u, \"failures\": %u},\n",(unsigned long long)arena_render_state.draws,(unsigned long long)arena_render_state.bow_draws,(unsigned long long)arena_render_state.arrow_draws,arena_render_state.bow_pose_assets,arena_render_state.failures);
            std::fprintf(f,"  \"milestone_review\": {\"enabled\": %s, \"view\": %d, \"portrait_frames\": %llu, \"focus_m\": %.4f, \"player_body\": %s},\n",milestone_finish?"true":"false",int(milestone_review.view),milestone_review.portrait_frames,portrait_focus_distance,milestone_review.portrait()&&third_person?"true":"false");
            std::fprintf(f,"  \"gpu_stage_timing_available\": %s,\n",profile_gpu?"true":"false");
            std::fprintf(f,"  \"trail_camp\": {\"active\": %s, \"actors\": %u, \"walking\": %u, \"seated\": %u, \"visible\": %u, \"pose_assets\": %u, \"failures\": %llu, \"unsafe_roots\": %llu},\n",trail_camp_state.active?"true":"false",trail_camp_state.actors,trail_camp_state.walking,trail_camp_state.seated,trail_camp_state.visible,trail_camp_state.pose_assets,(unsigned long long)trail_camp_state.failures,(unsigned long long)trail_camp_state.unsafe_roots);
            std::fprintf(f,"  \"arrival_camp_render\": {\"active\": %s, \"actors\": %u, \"walking\": %u, \"seated\": %u, \"visible\": %u, \"pose_assets\": %u, \"failures\": %llu, \"unsafe_roots\": %llu},\n",arrival_camp_state.active?"true":"false",arrival_camp_state.actors,arrival_camp_state.walking,arrival_camp_state.seated,arrival_camp_state.visible,arrival_camp_state.pose_assets,(unsigned long long)arrival_camp_state.failures,(unsigned long long)arrival_camp_state.unsafe_roots);
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
            std::fprintf(f,"  \"graphics\": {\"preset\": %s, \"automatic\": %s, \"target_fps\": %d, \"display_width\": %d, \"display_height\": %d, \"render_width\": %d, \"render_height\": %d, \"effective_scale\": %.3f, \"minimum_observed_scale\": %.3f, \"maximum_observed_scale\": %.3f, \"scale_changes\": %llu, \"accepted_gpu_samples\": %llu, \"excluded_gpu_samples\": %llu, \"stale_gpu_samples\": %llu, \"apply_failures\": %llu, \"applied_changes\": %llu, \"reflections\": %s, \"reflection_scale\": %.2f, \"contact_shadows\": %s, \"volumetric_fog\": %s, \"volumetric_clouds\": %s, \"floor_limited\": %s, \"cpu_limited\": %s},\n",
                world_json::quote(world_graphics_preset_name(graphics.preset)).c_str(),graphics.auto_quality?"true":"false",graphics.target_fps,display_w,display_h,graphics_render_w,graphics_render_h,
                float(graphics_render_w)/std::max(1,display_w),minimum_rendered_scale,maximum_rendered_scale,(unsigned long long)quality_reallocations,(unsigned long long)quality_state.accepted_samples,
                (unsigned long long)quality_state.excluded_samples,(unsigned long long)quality_state.stale_samples,(unsigned long long)quality_apply_failures,(unsigned long long)graphics_apply_count,
                graphics.reflections?"true":"false",graphics.reflection_scale,graphics.contact_shadows?"true":"false",graphics.volumetric_fog?"true":"false",
                graphics.clouds?"true":"false",quality_state.floor_limited?"true":"false",quality_state.cpu_limited?"true":"false");
            std::fprintf(f,"  \"designer_viewport\": {\"open\": %s, \"logical_width\": %d, \"logical_height\": %d, \"x\": %.3f, \"y\": %.3f, \"width\": %.3f, \"height\": %.3f, \"drawable_x\": %d, \"drawable_y\": %d, \"drawable_width\": %d, \"drawable_height\": %d, \"scene_width\": %d, \"scene_height\": %d, \"aspect\": %.6f, \"separate_camera\": %s},\n",
                studio.open?"true":"false",logical_w,logical_h,designer_view.x,designer_view.y,designer_view.w,designer_view.h,designer_drawable.x,designer_drawable.y,designer_drawable.w,designer_drawable.h,graphics_render_w,graphics_render_h,cam.aspect,designer_cameras.active?"true":"false");
            std::fprintf(f,"  \"hdr_bloom\": {\"enabled\": %s, \"strength\": %.6f, \"startup_ready\": %s, \"frames\": %zu, \"width\": %d, \"height\": %d, \"bytes\": %llu, \"allocations\": %llu, \"resolves\": %llu, \"failures\": %zu, \"scene_linear\": true, \"post_timing_includes_bloom\": true},\n",
                bloom_enabled?"true":"false",bloom_strength,bloom_startup_ready?"true":"false",bloom_play_frames,
                bloom_state.quarter_width,bloom_state.quarter_height,(unsigned long long)bloom_state.bytes,
                (unsigned long long)bloom_state.allocations,(unsigned long long)bloom_state.resolves,bloom_failures);
            std::fprintf(f,"  \"authored_depth_first\": {\"enabled\": %s, \"frames\": %zu, \"same_geometry\": true, \"same_materials\": true, \"single_submission\": true},\n",
                authored_first_enabled?"true":"false",authored_first_frames);
            std::fprintf(f,"  \"early_gpu_submission\": {\"enabled\": true, \"frames\": %zu, \"waits_for_completion\": false},\n",early_gpu_submitted_frames);
            std::fprintf(f,"  \"main_material_preparation\": {\"ready\": %s, \"calls\": %u, \"passes\": %u, \"startup_calls\": %u, \"destination_calls\": %u, \"failures\": %u, \"milliseconds\": %.3f, \"terrain_passes\": %u, \"static_model_passes\": %u, \"procedural_grass_passes\": %u, \"scanned_tuft_passes\": %u, \"near_tree_passes\": %u, \"pose_passes\": %u, \"headings_per_call\": 4, \"main_color_attachments\": 2, \"loading_only\": true, \"walking_fences\": 0, \"temporal_history_unchanged\": true},\n",
                main_material_warm.ready?"true":"false",main_material_warm.calls,main_material_warm.passes,
                main_material_warm.startup_calls,main_material_warm.destination_calls,main_material_warm.failures,main_material_warm.milliseconds,
                main_material_warm.terrain_passes,main_material_warm.static_passes,main_material_warm.grass_passes,
                main_material_warm.scanned_passes,main_material_warm.near_tree_passes,main_material_warm.pose_passes);
            std::fprintf(f,"  \"demo_tour\": {\"steps\": %u, \"qa_integration\": %s},\n",demo_tour_steps,qa_demo_tour?"true":"false");
            std::fprintf(f,"  \"atlas_map\": {\"loaded\": %s, \"attempts\": %u, \"visible\": %s, \"tools_visible\": %s, \"qa_events\": %u, \"lazy\": true},\n",
                atlas_map_loaded?"true":"false",atlas_map_attempted?1u:0u,show_map?"true":"false",show_tools?"true":"false",atlas_map_qa_events);
            std::fprintf(f,"  \"studio\": {\"applied\": %u, \"rejected\": %u, \"qa_phase\": %u, \"terrain_tiles\": %zu, \"added_models\": %zu, \"lights\": %zu, \"foliage_masks\": %zu, \"materials\": %zu, \"decals\": %zu, \"splines\": %zu, \"reused_model_groups\": %zu},\n",studio.applied,studio.rejected,qa_studio_phase,studio.live.terrain.size(),studio.live.placements.size(),studio.live.lights.size(),studio.live.stamps.size(),studio.live.materials.size(),studio.live.decals.size(),studio.live.splines.size(),authored_state.reused_groups);
            std::fprintf(f,"  \"source_terrain_materials\": {\"requested\": %s, \"enabled\": %s, \"layers\": %u, \"arrays\": %u, \"tile_side\": %d, \"weight_side\": %d, \"gpu_bytes\": %llu, \"source_uv_metres\": true, \"source_alpha_channels\": true},\n",
                showcase_terrain_requested?"true":"false",showcase_terrain_loaded?"true":"false",showcase_terrain_loaded?8u:0u,showcase_terrain_loaded?3u:0u,
                showcase_terrain_side,showcase_terrain_alpha_side,(unsigned long long)showcase_terrain_bytes);
            std::fprintf(f,"  \"surface_editors\": {\"phase\": %u, \"images\": %zu, \"layers\": %zu, \"paint_tiles\": %zu, \"projections\": %zu, \"winds\": %zu, \"paths\": %zu, \"river_meshes\": %zu, \"gpu_bytes\": %llu},\n",qa_surface_phase,studio.live.surfaces.images.size(),studio.live.surfaces.layers.size(),surface_paint_count,studio.live.surfaces.projections.size(),studio.live.surfaces.winds.size(),studio.live.surfaces.ribbons.size(),surface_river_count,(unsigned long long)surface_gpu_bytes);
            std::fprintf(f,"  \"runtime_editors\": {\"phase\": %u, \"commits\": %u, \"actors\": %zu, \"navigation_areas\": %zu, \"particles\": %zu, \"waters\": %zu, \"graphs\": %zu, \"graph_button_compiles\": %u},\n",qa_runtime_phase,studio.runtime_commits,studio.live.actors.size(),studio.live.navigation.size(),studio.live.particles.size(),studio.live.waters.size(),studio.live.graphs.size(),studio.graph_compiles);
            const auto &editor_index=studio.designer_scene_index.counters();
            std::fprintf(f,"  \"editor_optimization\": {\"source_filter_rebuilds\": %llu, \"authored_row_rebuilds\": %llu, \"topology_rebuilds\": %llu, \"inspector_refreshes\": %llu, \"legacy_baseline_copies\": %llu, \"pick_index_rebuilds\": %u, \"last_pick_exact_tests\": %u, \"last_pick_nodes\": %u, \"pick_mesh_cache_hits\": %llu, \"pick_mesh_cache_misses\": %llu},\n",
                (unsigned long long)editor_index.source_filter_rebuilds,(unsigned long long)editor_index.authored_row_rebuilds,(unsigned long long)editor_index.topology_rebuilds,(unsigned long long)studio.designer_inspector_refreshes,(unsigned long long)studio.designer_baseline_copies,studio.picking_index_rebuilds,studio.picking_exact_checks,studio.picking_index_nodes,(unsigned long long)studio.picking_cache_hits,(unsigned long long)studio.picking_cache_misses);
            std::fprintf(f,"  \"production_editor\": {\"active_tool\": %d, \"entities\": %zu, \"id_highwater\": %llu, \"project_saved\": %s, \"qa_enabled\": %s, \"qa_phase\": %u, \"qa_checks\": %u, \"qa_failed\": %s},\n",studio.tab,studio.live.entities.nodes.size(),(unsigned long long)studio.production_id_highwater,studio.production_project_saved?"true":"false",qa_production?"true":"false",qa_production_phase,qa_production_checks,qa_production_failed?"true":"false");
            std::fprintf(f,"  \"city_visible_sky\": {\"enabled\": %s, \"gpu_bytes\": %llu, \"material_ibl_unchanged\": true, \"local_probe_background_unchanged\": true},\n",city_visible_sky_enabled?"true":"false",(unsigned long long)city_visible_sky_bytes);
            std::fprintf(f,"  \"studio_tools\": {\"document_version\": 5, \"groups\": %zu, \"asset_definitions\": %zu, \"gizmo_drags\": %u, \"gizmo_finishes\": %u, \"mesh_pick_requests\": %u, \"hdr_enabled\": %s, \"hdr_side\": %u, \"hdr_levels\": %u, \"hdr_gpu_bytes\": %llu},\n",studio.live.groups.size(),studio.live.assets.size(),studio.gizmo_drags,studio.gizmo_finishes,studio.pick_requests,studio.live.environment.enabled?"true":"false",environment_state.side,environment_state.levels,(unsigned long long)environment_state.bytes);
            const double draft_dx=studio.draft.placements.empty()||studio.live.placements.empty()?0:studio.draft.placements.front().x-studio.live.placements.front().x;
            std::fprintf(f,"  \"studio_input\": {\"mesh_hits\": %u, \"ui_undos\": %u, \"draft_first_delta_x\": %.8f, \"history_back\": %zu, \"history_forward\": %zu},\n",studio.mesh_hits,studio.ui_undos,draft_dx,studio.history.back.size(),studio.history.forward.size());
            std::fprintf(f,"  \"local_probes\": {\"enabled\": %s, \"cache_regions\": %u, \"count\": %u, \"ready\": %u, \"side\": 128, \"bytes\": %llu, \"capture_calls\": %llu, \"faces\": %llu, \"readbacks\": %llu, \"bind_calls\": %llu, \"failures\": %llu, \"loading_bakes\": %u, \"walking_bakes\": 0, \"ordinary_mip_reflections\": true, \"sh9_diffuse\": true},\n",
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
                WORLD_COVER_GRASS_CAP*2,dressing.peak_blended_grass_roots,dressing.grass_blend_updates,
                Dressing::scanned_group_capacity,Dressing::scanned_batch_capacity,dressing.peak_scanned_group_candidates,dressing.scanned_grass_cap_drops);
            std::fprintf(f,"  \"local_atmosphere\": %s, \"village_atmosphere_weight\": %.6f,\n",
                (last_rain>0||last_fog>0)?"true":"false",world_atlas_village_weight(cam.x,cam.z));
            std::fprintf(f,"  \"weather_cover_queries\": %llu, \"rain_splash_vertices_cap\": 38400,\n",(unsigned long long)weather_state.cover_queries);
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
            std::fprintf(f,"  \"character_polish\": {\"enabled\": %s, \"pose_assets\": %u, \"reaction_assets\": %u, \"death_assets\": %u, \"keep_defeated\": %s, \"fallen_peak\": %u, \"death_starts\": %llu, \"directional_reactions\": %llu, \"fall_checked\": %s, \"grounded_checked\": %s, \"grounded_age\": %.6f},\n",combat_motion_state.polished?"true":"false",combat_motion_state.pose_assets,combat_motion_state.reaction_assets,combat_motion_state.death_assets,arena_game.keep_defeated?"true":"false",combat_motion_state.peak_fallen_instances,(unsigned long long)combat_motion_state.death_starts,(unsigned long long)combat_motion_state.directional_reactions,arena_qa_character_fall_checked?"true":"false",arena_qa_character_grounded_checked?"true":"false",arena_qa_character_grounded_age);
            std::fprintf(f,"  \"combat_feedback\": {\"body_bursts\": %u, \"metal_bursts\": %u, \"body_audio\": %u, \"metal_audio\": %u, \"cut_windup_seconds\": %.3f, \"cut_cycle_seconds\": %.3f, \"parry_window_seconds\": %.3f},\n",combat_vfx_state.body_bursts,combat_vfx_state.metal_bursts,training_body_audio,training_metal_audio,WorldTrainingCombat::cut_windup_seconds,WorldTrainingCombat::cut_windup_seconds+WorldTrainingCombat::player_strike_seconds+WorldTrainingCombat::player_recovery_seconds,WorldTrainingCombat::parry_window_seconds);
            std::fprintf(f,"  \"volumetric_blood\": {\"enabled\": %s, \"provenance_loaded\": %s, \"variants\": %u, \"source_frames\": %u, \"source_vertices\": %llu, \"source_triangles\": %llu, \"geometry_bytes\": %llu, \"texture_bytes\": %llu, \"mesh_draws\": %llu, \"stain_draws\": %llu, \"frame_transitions\": %llu, \"failures\": %llu, \"spawned\": %llu, \"peak_meshes\": %u, \"peak_stains\": %u, \"counter_rebases\": %llu, \"resets\": %llu},\n",blood_enabled?"true":"false",blood_render_state.provenance_loaded?"true":"false",blood_render_state.loaded_variants,blood_render_state.source_frames,(unsigned long long)blood_render_state.source_vertices,(unsigned long long)blood_render_state.triangles,(unsigned long long)blood_render_state.geometry_bytes,(unsigned long long)blood_render_state.texture_bytes,(unsigned long long)blood_render_state.mesh_draws,(unsigned long long)blood_render_state.stain_draws,(unsigned long long)blood_render_state.frame_transitions,(unsigned long long)blood_render_state.failures,(unsigned long long)blood_state.mesh_spawned,blood_render_state.peak_meshes,blood_render_state.peak_stains,(unsigned long long)blood_state.counter_rebases,(unsigned long long)blood_state.resets);
            std::fprintf(f,"  \"combat_blood_intensity\": {\"fluid_scale\": %.2f, \"initial_particles\": %u, \"impacts\": %llu, \"secondary_sprays\": %llu, \"secondary_draws\": %llu, \"finisher_events\": %llu, \"finisher_draws\": %llu, \"initial_finisher_bursts\": %u, \"max_strength\": %.3f},\n",WorldBloodFrame{}.scale,world_combat_vfx::outgoing_body_particles,(unsigned long long)blood_state.impacts_spawned,(unsigned long long)blood_state.secondary_spawned,(unsigned long long)blood_render_state.secondary_draws,(unsigned long long)blood_state.finisher_events,(unsigned long long)blood_render_state.finisher_draws,combat_vfx_state.finisher_bursts,blood_state.max_strength);
            std::fprintf(f,"  \"ground_blood_persistence\": {\"enabled\": %s, \"capacity\": %u, \"witness_checked\": %s, \"witness_age\": %.6f, \"witness_serial\": %llu, \"stains_spawned\": %llu, \"stains_replaced\": %llu},\n",blood_effects.config().persistent_stains?"true":"false",world_blood_effects::stain_capacity,arena_qa_blood_persistent_checked?"true":"false",arena_qa_blood_persistent_age,(unsigned long long)arena_qa_blood_floor_serial,(unsigned long long)blood_state.stains_spawned,(unsigned long long)blood_state.stains_replaced);
            std::fprintf(f,"  \"combat_blood_motion\": {\"playback_rate\": %.2f, \"maximum_mesh_seconds\": %.6f, \"gravity\": %.2f, \"mist_lifetime\": %.2f, \"settled_checked\": %s, \"settle_elapsed\": %.6f, \"settle_stains\": %u},\n",world_blood_effects::playback_rate,blood_effects.config().mesh_duration+.035,world_combat_vfx::blood_gravity,world_combat_vfx::blood_mist_lifetime,arena_qa_blood_settled_checked?"true":"false",arena_qa_blood_settle_elapsed,arena_qa_blood_settle_stains);
            std::fprintf(f,"  \"camera_view\": {\"third_person\": %s, \"toggles\": %u},\n",third_person?"true":"false",camera_toggles);
            std::fprintf(f,"  \"rainforest_weight\": %.6f,\n",world_atlas_rainforest_weight(cam.x,cam.z));
            std::fprintf(f,"  \"zero_coverage_culling\": {\"color_enabled\": %s, \"shadow_enabled\": false, \"guard_pixels\": 2.99, \"existing_zero_coverage_endpoint_pixels\": 3},\n",zero_coverage_cull_enabled?"true":"false");
            std::fprintf(f,"  \"zero_contribution_work_skip\": {\"terrain_noise_existing_endpoints\": true, \"primary_probe_exact_zero\": true, \"opaque_zero_sun_nine_tap_only\": true, \"leaf_transmission_unchanged\": true},\n");
            std::fprintf(f,"  \"shadow_fetch_grouping\": {\"supported\": %s, \"enabled\": %s, \"logical_taps\": 9, \"scalar_border_fallback\": true, \"four_tap_unchanged\": true},\n",
                shadow_gather_supported?"true":"false",shadow_gather_enabled?"true":"false");
            std::fprintf(f,"  \"vegetation_selection_work\": {\"grass_classification_refreshes\": %llu, \"grass_classified_seeds\": %llu, \"grass_structural_revision\": %llu, \"tree_dispatches\": %llu, \"grass_dispatches\": %llu, \"tree_fallbacks\": %llu, \"grass_fallbacks\": %llu, \"density_ranges_caps_order_unchanged\": true},\n",
                (unsigned long long)dressing.grass.classification_refreshes,(unsigned long long)dressing.grass.classified_seeds,
                (unsigned long long)dressing.grass.classification_revision,(unsigned long long)dressing.tree_dispatches,
                (unsigned long long)dressing.grass_dispatches,(unsigned long long)dressing.tree_fallbacks,
                (unsigned long long)dressing.grass_fallbacks);
            std::fprintf(f,"  \"wildlife_material_preparation\": {\"enabled\": %s, \"calls\": %llu, \"main_passes\": %llu, \"reflection_passes\": %llu, \"shadow_passes\": %llu, \"pose_draws\": %llu, \"failures\": %llu, \"loading_only\": true, \"main_color_attachments\": 2, \"reflection_color_attachments\": 1, \"walking_fences\": 0},\n",
                wildlife_preparation_enabled?"true":"false",(unsigned long long)wildlife_preparation.calls,
                (unsigned long long)wildlife_preparation.main_passes,(unsigned long long)wildlife_preparation.reflection_passes,
                (unsigned long long)wildlife_preparation.shadow_passes,(unsigned long long)wildlife_preparation.pose_draws,
                (unsigned long long)wildlife_preparation.failures);
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
            std::fprintf(f,"  \"village_torches\": {\"models\": %zu, \"emitters\": %u, \"light_cap\": %u, \"peak_lights\": %zu, \"peak_visible_flames\": %zu, \"flame_draws\": %llu, \"main_frames\": %zu, \"reflection_frames\": %zu, \"buffer_bytes\": %zu, \"failures\": %zu, \"shared_smooth_flicker\": true, \"wall_plane_and_hall_clip\": true, \"point_shadow_maps\": false},\n",
                std::size(village_torch_emitters),unsigned(std::size(village_torch_emitters)),WorldTorchLights::capacity,torch_peak_lights,torch_peak_visible,
                (unsigned long long)torch_state.draws,torch_main_frames,torch_reflection_frames,torch_state.gpu_buffer_bytes,torch_failures);
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
            std::fprintf(f,"  \"forest_shelter\": {\"enabled\": %s, \"trees\": %u, \"side\": 513, \"gpu_mib\": %.6f, \"minimum_access\": %.6f, \"mean_access\": %.6f, \"startup_only\": true, \"extra_samplers\": 0, \"extra_shadow_passes\": 0},\n",
                landscape.forest_shelter_enabled?"true":"false",landscape.forest_shelter.trees,513.*513.*4./1048576.,
                landscape.forest_shelter.minimum_access,landscape.forest_shelter.mean_access);
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
  "walking_capsule_radius_m": 0.37, "manual_collision_steps": %zu, "manual_slope_steps": %zu, "manual_sliding_steps": %zu,
  "starting_mask_side": 513, "starting_mask_gpu_mib": %.6f,
  "authored_source_files": %zu, "authored_unique_aliases": %zu, "authored_colliders": %zu,
  "benchmark_route": {"active": %s, "id": %s, "length_m": %.6f,
    "speed_mps": %.3f, "step_seconds": %.9f, "walking_frames": %zu, "movement_commands": %zu,
    "elapsed_sim_seconds": %.6f, "attempted_distance_m": %.6f, "actual_distance_m": %.6f,
    "waypoints_reached": %zu, "collision_blocked_steps": %zu, "slope_blocked_steps": %zu,
    "sliding_steps": %zu, "waiting_frames": %zu, "complete": %s, "return_error_m": %.6f},
  "authored_sources": [
)JSON",world_json::quote(launch->scenic).c_str(),world_json::quote(launch->view).c_str(),launch->benchmark?"true":"false",
                cam.x,cam.y,cam.z,cam.yaw,cam.pitch,cam.fly?"true":"false",
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

