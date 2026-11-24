/* Run from the engine root, or pass that root as the only argument. Real
 * project assets stay in their source project; the atlas builder exposes them
 * through per-file links in projects/valwood_world/models.
 *
 * Link this test with engine/world_models.cpp and engine/texture_cache.c,
 * without main.c. These implementations provide the loader dependencies.
 * Optional query-count validation needs both
 * -DWORLD_MODELS_TEST_WRAP_UNIFORM_LOCATION and
 * -Wl,--wrap=glGetUniformLocation (GNU-compatible linker).
 */
#define GL_GLEXT_PROTOTYPES
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <SDL2/SDL.h>
#include "gl_helpers.h"
#include "engine/world_models.h"
extern "C" {
#include "engine/texture_cache.h"
}

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <limits.h>
#include <new>
#include <sstream>
#include <string>
#include <vector>
#include <unistd.h>

namespace {
thread_local bool track_upload_allocations=false;
thread_local size_t upload_allocations=0;
}
[[gnu::noinline]] void *operator new(std::size_t size) {
    if(track_upload_allocations)++upload_allocations;
    if(void *memory=std::malloc(size?size:1))return memory;
    throw std::bad_alloc();
}
[[gnu::noinline]] void operator delete(void *memory) noexcept {std::free(memory);}
[[gnu::noinline]] void operator delete(void *memory,std::size_t) noexcept {std::free(memory);}

#ifdef WORLD_MODELS_TEST_WRAP_UNIFORM_LOCATION
static unsigned uniform_queries;
extern "C" GLint __real_glGetUniformLocation(GLuint, const GLchar *);
extern "C" GLint __wrap_glGetUniformLocation(GLuint program, const GLchar *name) {
    ++uniform_queries;
    return __real_glGetUniformLocation(program, name);
}
#endif

namespace {
constexpr int target_size = 96;

struct FixtureConstraints {
    const char *name;
    size_t triangles, images;
    float span[3];
    float tree_height;
    bool shared_with_previous;
};

const FixtureConstraints new_fixtures[] = {
    {"cinematic_broadleaf_near", 18300, 9, {6.426128f, 10.f, 9.356057f}, 0, false},
    {"cinematic_broadleaf_mid", 3249, 9, {6.288463f, 10.f, 9.313155f}, 0, true},
    {"cinematic_broadleaf_far", 2, 1, {6.444141f, 10.019531f, 0.f}, 10.f, false},
    {"cinematic_pine_young_near", 17667, 6, {1.616775f, 3.2f, 2.232877f}, 0, false},
    {"cinematic_pine_young_mid", 3100, 6, {1.613312f, 3.2f, 2.230615f}, 0, true},
    {"cinematic_pine_young_far", 2, 1, {1.626750f, 3.209625f, 0.f}, 3.2f, false},
    {"cinematic_deadwood", 6500, 3, {4.057317f, 1.054324f, 1.055085f}, 0, false},
    {"cinematic_deadwood_mid", 1500, 3, {4.053185f, 1.045130f, 1.037848f}, 0, true}
};

const FixtureConstraints *constraints_for(const char *name) {
    for (const auto &fixture : new_fixtures)
        if (std::strcmp(fixture.name, name) == 0) return &fixture;
    return nullptr;
}

void check_image(const cgltf_image *image, int limit) {
    assert(image && !image->uri && image->buffer_view && image->buffer_view->size <= INT_MAX);
    const uint8_t *encoded = cgltf_buffer_view_data(image->buffer_view);
    int width = 0, height = 0, components = 0;
    assert(encoded && stbi_info_from_memory(encoded, int(image->buffer_view->size), &width, &height, &components));
    assert(width > 0 && height > 0 && width <= limit && height <= limit);
}

void check_view(const cgltf_texture_view &view, int limit) {
    if (!view.texture) return;
    assert(view.texcoord == 0 && !view.has_transform);
    check_image(view.texture->image, limit);
}

/* Independently inspect the source GLB, rather than only trusting the runtime
   loader's limits. The eight new fixture dimensions are evaluated vertex/node
   bounds, not Blender's sometimes stale decimation bounding-box metadata. */
void check_fixture_source(const std::string &path, const char *name,
                          const FixtureConstraints *fixture, const float lo[3], const float hi[3]) {
    cgltf_options options = {};
    cgltf_data *data = nullptr;
    assert(cgltf_parse_file(&options, path.c_str(), &data) == cgltf_result_success && data);
    assert(cgltf_load_buffers(&options, data, path.c_str()) == cgltf_result_success);
    assert(cgltf_validate(data) == cgltf_result_success);
    assert(data->file_type == cgltf_file_type_glb && !data->skins_count && !data->animations_count);
    for (cgltf_size i = 0; i < data->images_count; ++i) check_image(&data->images[i], 2048);
    size_t triangles = 0;
    for (cgltf_size mi = 0; mi < data->meshes_count; ++mi) {
        const cgltf_mesh &mesh = data->meshes[mi];
        for (cgltf_size pi = 0; pi < mesh.primitives_count; ++pi) {
            const cgltf_primitive &primitive = mesh.primitives[pi];
            assert(primitive.type == cgltf_primitive_type_triangles && primitive.indices);
            assert(primitive.indices->count % 3 == 0);
            triangles += primitive.indices->count / 3;
            bool uv0 = false;
            for (cgltf_size ai = 0; ai < primitive.attributes_count; ++ai) {
                const cgltf_attribute &a = primitive.attributes[ai];
                if (a.type == cgltf_attribute_type_texcoord && a.index == 0) uv0 = true;
            }
            assert(uv0 && primitive.material);
        }
    }
    for (cgltf_size mi = 0; mi < data->materials_count; ++mi) {
        const cgltf_material &material = data->materials[mi];
        check_view(material.pbr_metallic_roughness.base_color_texture, 2048);
        check_view(material.normal_texture, fixture ? 1024 : 2048);
        check_view(material.pbr_metallic_roughness.metallic_roughness_texture, fixture ? 512 : 2048);
    }
    if (fixture) {
        assert(triangles == fixture->triangles && data->images_count == fixture->images);
        float raw_lo[3] = {INFINITY, INFINITY, INFINITY};
        float raw_hi[3] = {-INFINITY, -INFINITY, -INFINITY};
        for (cgltf_size ni = 0; ni < data->nodes_count; ++ni) {
            const cgltf_node &node = data->nodes[ni];
            if (!node.mesh) continue;
            float matrix[16];
            cgltf_node_transform_world(&node, matrix);
            for (cgltf_size pi = 0; pi < node.mesh->primitives_count; ++pi) {
                const cgltf_primitive &primitive = node.mesh->primitives[pi];
                for (cgltf_size ai = 0; ai < primitive.attributes_count; ++ai) {
                    const cgltf_attribute &attribute = primitive.attributes[ai];
                    if (attribute.type != cgltf_attribute_type_position) continue;
                    for (cgltf_size vi = 0; vi < attribute.data->count; ++vi) {
                        float p[3];
                        assert(cgltf_accessor_read_float(attribute.data, vi, p, 3));
                        for (int k = 0; k < 3; ++k) {
                            const float world = matrix[k]*p[0] + matrix[4+k]*p[1] + matrix[8+k]*p[2] + matrix[12+k];
                            assert(std::isfinite(world));
                            raw_lo[k] = std::min(raw_lo[k], world);
                            raw_hi[k] = std::max(raw_hi[k], world);
                        }
                    }
                }
            }
        }
        for (int k = 0; k < 3; ++k) {
            assert(std::fabs((hi[k]-lo[k])-fixture->span[k]) < .003f);
            assert(std::fabs((raw_hi[k]-raw_lo[k])-(hi[k]-lo[k])) < .003f);
        }
        if (fixture->tree_height > 0) {
            assert(triangles == 2 && data->meshes_count == 1 && data->materials_count == 1);
            assert(hi[2] == lo[2]);
            assert(data->materials[0].alpha_mode == cgltf_alpha_mode_mask && data->materials[0].double_sided);
            const cgltf_image *image = data->materials[0].pbr_metallic_roughness.base_color_texture.texture->image;
            int width, height, components;
            const uint8_t *encoded = cgltf_buffer_view_data(image->buffer_view);
            unsigned char *rgba = stbi_load_from_memory(encoded, int(image->buffer_view->size), &width, &height, &components, 4);
            assert(rgba && width > 2 && height > 2);
            for (size_t corner : {size_t(0), size_t(width-1), size_t(width*(height-1)), size_t(width*height-1)})
                assert(rgba[corner*4+3] < 16); // No opaque canvas corners.
            stbi_image_free(rgba);
        }
    }
    std::printf("Fixture %s: %zu triangles, %zu embedded images%s\n", name, triangles,
                size_t(data->images_count), fixture ? ", source constraints passed" : "");
    cgltf_free(data);
}

std::string read_source(const std::string &path) {
    std::ifstream file(path.c_str());
    if (!file) std::fprintf(stderr, "Missing shader fixture: %s\n", path.c_str());
    assert(file);
    std::ostringstream source;
    source << file.rdbuf();
    assert(!source.str().empty());
    return source.str();
}

GLuint load_program(const std::string &vertex, const std::string &fragment) {
    const std::string vs = read_source(vertex), fs = read_source(fragment);
    const GLuint program = GLH_program(vs.c_str(), fs.c_str());
    GLint linked = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    assert(program && linked);
    glUseProgram(program);glUniform1i(glGetUniformLocation(program,"uVillageSkyMask"),12);
    glUniform1i(glGetUniformLocation(program,"uLocalProbe0"),4);glUniform1i(glGetUniformLocation(program,"uLocalProbe1"),5);
    glUniform1i(glGetUniformLocation(program,"uLocalProbeCount"),0);glUniform1f(glGetUniformLocation(program,"uLocalIrradianceWeight"),0);
    glUniform1i(glGetUniformLocation(program,"uVillageSkyDirectional"),13);glUniform1i(glGetUniformLocation(program,"uVillageSkyUp"),14);
    return program;
}

std::size_t check_material_samplers(WorldModelAsset *asset,const std::string &path,GLuint program) {
    cgltf_options options={};cgltf_data *data=nullptr;
    assert(cgltf_parse_file(&options,path.c_str(),&data)==cgltf_result_success);
    const cgltf_scene *scene=data->scene?data->scene:(data->scenes_count?data->scenes:nullptr);
    const cgltf_material *last=nullptr;
    for(std::size_t n=0;n<data->nodes_count;n++) {
        const auto *node=data->nodes+n;if(!node->mesh)continue;
        bool active=!scene;
        for(const auto *parent=node;parent&&!active;parent=parent->parent)
            for(std::size_t k=0;k<scene->nodes_count;k++)active=active||scene->nodes[k]==parent;
        if(active)for(std::size_t k=0;k<node->mesh->primitives_count;k++)last=node->mesh->primitives[k].material;
    }
    std::size_t checked=0;
    if(last) {
        world_model_draw(asset,program);
        const cgltf_texture_view *views[]={&last->pbr_metallic_roughness.base_color_texture,
            &last->normal_texture,&last->pbr_metallic_roughness.metallic_roughness_texture};
        const bool supported=SDL_GL_ExtensionSupported("GL_EXT_texture_filter_anisotropic")||
            SDL_GL_ExtensionSupported("GL_ARB_texture_filter_anisotropic");
        float maximum=1;if(supported)glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT,&maximum);
        for(GLuint unit:{0u,8u,9u,10u}){GLint bound=0;glGetIntegeri_v(GL_SAMPLER_BINDING,unit,&bound);assert(bound==0);}
        float strength=-1;glGetUniformfv(program,glGetUniformLocation(program,"uNormalScale"),&strength);
        assert(std::abs(strength-last->normal_texture.scale)<.00001f);
        GLint previous_active=0;glGetIntegerv(GL_ACTIVE_TEXTURE,&previous_active);
        for(unsigned unit=0;unit<3;unit++)if(views[unit]->texture) {
            glActiveTexture(GL_TEXTURE0+unit);GLint texture=0;glGetIntegerv(GL_TEXTURE_BINDING_2D,&texture);assert(texture);
            const cgltf_sampler *sampler=views[unit]->texture->sampler;
            const GLint expected[]={sampler&&sampler->min_filter?GLint(sampler->min_filter):GL_LINEAR_MIPMAP_LINEAR,
                sampler&&sampler->mag_filter?GLint(sampler->mag_filter):GL_LINEAR,
                sampler&&sampler->wrap_s?GLint(sampler->wrap_s):GL_REPEAT,
                sampler&&sampler->wrap_t?GLint(sampler->wrap_t):GL_REPEAT};
            const GLenum parameters[]={GL_TEXTURE_MIN_FILTER,GL_TEXTURE_MAG_FILTER,GL_TEXTURE_WRAP_S,GL_TEXTURE_WRAP_T};
            for(int k=0;k<4;k++){GLint actual=0;glGetTexParameteriv(GL_TEXTURE_2D,parameters[k],&actual);assert(actual==expected[k]);}
            if(supported){float actual=0;glGetTexParameterfv(GL_TEXTURE_2D,GL_TEXTURE_MAX_ANISOTROPY_EXT,&actual);const float expected_anisotropy=maximum>=2?float(unsigned(std::min(4.f,maximum))):1.f;
                assert(std::fabs(actual-expected_anisotropy)<.001f);}
            checked++;
        }
        glActiveTexture(GLenum(previous_active));
    }
    cgltf_free(data);assert(glGetError()==GL_NO_ERROR);return checked;
}

void configure_program(GLuint program, const float lo[3], const float hi[3], GLuint white_depth) {
    float extent = 0;
    for (int i = 0; i < 3; ++i) extent = std::max(extent, hi[i] - lo[i]);
    assert(extent > 0);
    const float scale = 1.f / (extent * .85f);
    const float projection[16] = {
        scale, 0, 0, 0, 0, scale, 0, 0, 0, 0, scale, 0, 0, 0, 0, 1
    };
    const float identity[16] = {
        1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1
    };
    glUseProgram(program);
    glUniformMatrix4fv(glGetUniformLocation(program, "uVP"), 1, GL_FALSE, projection);
    glUniform3f(glGetUniformLocation(program, "uOffset"),
                -(lo[0] + hi[0]) * .5f, -(lo[1] + hi[1]) * .5f, -(lo[2] + hi[2]) * .5f);
    glUniform1f(glGetUniformLocation(program, "uTime"), 17.f);
    glUniform1f(glGetUniformLocation(program, "uWind"), 0.f);
    glUniform1f(glGetUniformLocation(program, "uTemporalPhase"), 0.f);
    glUniform1f(glGetUniformLocation(program, "uTreeHeight"), 25.f);
    glUniform2f(glGetUniformLocation(program, "uWorldAnchor"), 0.f, 0.f);
    glUniform2f(glGetUniformLocation(program, "uWorldSize"), 100800.f, 56708.f);
    glUniformMatrix4fv(glGetUniformLocation(program, "uLightVP"), 1, GL_FALSE, identity);
    glUniform1i(glGetUniformLocation(program, "uGrass"), 0);
    glUniform1i(glGetUniformLocation(program, "uVegetation"), 0);
    glUniform1i(glGetUniformLocation(program, "uLodLevel"), -1);
    glUniform3f(glGetUniformLocation(program, "uSun"), .4f, .8f, .4f);
    glUniform3f(glGetUniformLocation(program, "uSunColor"), 1.f, .95f, .8f);
    glUniform3f(glGetUniformLocation(program, "uAmbient"), .65f, .65f, .65f);
    glUniform3f(glGetUniformLocation(program, "uFog"), .2f, .3f, .4f);
    glUniform1f(glGetUniformLocation(program, "uHaze"), 0.f);
    glUniform1i(glGetUniformLocation(program, "uShadow"), 7);
    glActiveTexture(GL_TEXTURE7);
    glBindTexture(GL_TEXTURE_2D, white_depth);
}

void check_instances(WorldModelAsset *asset) {
    const WorldModelInstance instance = {0, 0, 0, 0, 1};
    upload_allocations=0;track_upload_allocations=true;
    assert(world_model_upload_instances(asset, &instance, 1));
    track_upload_allocations=false;assert(upload_allocations==0);
    assert(world_model_instance_count(asset) == 1);
    WorldModelInstance invalid = instance;
    invalid.scale = std::numeric_limits<float>::quiet_NaN();
    assert(!world_model_upload_instances(asset, &invalid, 1));
    invalid = instance;
    invalid.x = std::numeric_limits<float>::infinity();
    assert(!world_model_upload_instances(asset, &invalid, 1));
    invalid = instance;
    invalid.scale = 0;
    assert(!world_model_upload_instances(asset, &invalid, 1));
    invalid.scale = -1;
    assert(!world_model_upload_instances(asset, &invalid, 1));
    assert(!world_model_upload_instances(asset, nullptr, 1));
    assert(!world_model_upload_instances(asset, &instance, 4097));
    assert(world_model_instance_count(asset) == 1);
}

void check_reusable_uploads(WorldModelAsset *asset) {
    std::array<WorldModelInstance,4096> instances;
    std::array<float,4096> fades;
    for(size_t i=0;i<instances.size();++i)instances[i]={float(i)*.01f,0,0,float(i)*.001f,1};
    fades.fill(.5f);
    // Include the first maximum-size upload, shrink, clear, refill and repeated
    // prefix changes. An upload-local vector would allocate on every nonzero
    // call; late reservation would allocate at the maximum-size transition.
    upload_allocations=0;track_upload_allocations=true;
    for(size_t count:{size_t(4096),size_t(1),size_t(17),size_t(0),size_t(255),size_t(4095),size_t(2)}) {
        assert(world_model_upload_instances(asset,instances.data(),count));
        assert(world_model_instance_count(asset)==count);
        assert(world_model_upload_fading_instances(asset,instances.data(),fades.data(),count));
        assert(world_model_instance_count(asset)==count);
    }
    track_upload_allocations=false;assert(upload_allocations==0);
    GLuint sentinel=0;glGenBuffers(1,&sentinel);glBindBuffer(GL_ARRAY_BUFFER,sentinel);
    instances.back().scale=std::numeric_limits<float>::quiet_NaN();
    upload_allocations=0;track_upload_allocations=true;
    assert(!world_model_upload_instances(asset,instances.data(),instances.size()));
    track_upload_allocations=false;assert(upload_allocations==0&&world_model_instance_count(asset)==2);
    GLint binding=0;glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&binding);assert(GLuint(binding)==sentinel);
    assert(!world_model_upload_instances(asset,instances.data(),4097));
    assert(!world_model_upload_instances(asset,nullptr,1));
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&binding);assert(GLuint(binding)==sentinel);
    assert(world_model_instance_count(asset)==2);
    glBindBuffer(GL_ARRAY_BUFFER,0);glDeleteBuffers(1,&sentinel);
    const WorldModelInstance original={0,0,0,0,1};assert(world_model_upload_instances(asset,&original,1));
    assert(glGetError()==GL_NO_ERROR);
    std::puts("PASS model upload staging: first/max/varied/clear/refill uploads allocate zero C++ memory; late-invalid input preserves count and GL bindings");
}

void check_program_cache(WorldModelAsset *asset, GLuint color, GLuint shadow) {
#ifdef WORLD_MODELS_TEST_WRAP_UNIFORM_LOCATION
    const unsigned before = uniform_queries;
#endif
    /* No fragment work is needed to test repeated program changes. Both
       actual rendering paths are separately checked below with readback. */
    glEnable(GL_RASTERIZER_DISCARD);
    for (unsigned i = 0; i < 12; ++i) world_model_draw(asset, (i % 2) ? shadow : color);
    glDisable(GL_RASTERIZER_DISCARD);
#ifdef WORLD_MODELS_TEST_WRAP_UNIFORM_LOCATION
    assert(uniform_queries - before == 42);  // Twenty-one cached locations per program, including source normal strength.
#endif
    assert(glGetError() == GL_NO_ERROR);
}

void check_color(WorldModelAsset *asset, GLuint program, bool expect_visible = true) {
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    world_model_draw(asset, program);
    std::vector<unsigned char> pixels(target_size * target_size * 4);
    glReadPixels(0, 0, target_size, target_size, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    size_t lit = 0;
    for (size_t i = 0; i < pixels.size(); i += 4)
        if (pixels[i] || pixels[i + 1] || pixels[i + 2]) ++lit;
    if (expect_visible) assert(lit > 5);
    else assert(lit == 0);
    assert(glGetError() == GL_NO_ERROR);
}

void check_shadow(WorldModelAsset *asset, GLuint program, bool expect_visible=true) {
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glClear(GL_DEPTH_BUFFER_BIT);
    world_model_draw(asset, program);
    std::vector<float> pixels(target_size * target_size);
    glReadPixels(0, 0, target_size, target_size, GL_DEPTH_COMPONENT, GL_FLOAT, pixels.data());
    glClear(GL_DEPTH_BUFFER_BIT);
    {
        WorldModelDepthMaterialScope depth_materials(program);
        world_model_draw(asset,program);
        {WorldModelDepthMaterialScope ordinary(0);}
        world_model_draw(asset,program);
    }
    std::vector<float> optimized(pixels.size());
    glReadPixels(0,0,target_size,target_size,GL_DEPTH_COMPONENT,GL_FLOAT,optimized.data());
    assert(optimized==pixels);
    assert(world_model_depth_material_program(0)==0);
    size_t covered = 0;
    for (float depth : pixels) {
        assert(std::isfinite(depth) && depth >= 0.f && depth <= 1.f);
        if (depth < .999f) ++covered;
    }
    if(expect_visible)assert(covered > 5);else assert(covered==0);
    assert(glGetError() == GL_NO_ERROR);
}

void check_source_alpha_modes(GLuint color,GLuint shadow,GLuint white_depth) {
    // A real embedded PNG stores useful orange RGB with alpha0. OPAQUE must
    // render it in color and depth; MASK must reject it. This directly catches
    // the former universal .4 cutoff that punched holes in opaque thatch.
    const unsigned char png[]={137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,16,0,0,0,16,8,6,0,0,0,31,243,255,97,0,0,0,28,73,68,65,84,120,156,99,252,208,99,195,64,9,96,162,72,247,168,1,163,6,140,26,48,136,12,0,0,62,61,1,216,166,185,127,92,0,0,0,0,73,69,78,68,174,66,96,130};
    const float positions[]={-.8f,0,0,.8f,0,0,0,1.2f,0};
    const float normals[]={0,0,1,0,0,1,0,0,1},uv[]={.5f,.5f,.5f,.5f,.5f,.5f};
    const uint16_t indices[]={0,1,2};
    for(const char *mode:{"OPAQUE","MASK","BLEND"}) {
        std::vector<unsigned char> binary;
        auto append=[&](const void *data,std::size_t size) {
            while(binary.size()%4)binary.push_back(0);
            const std::size_t offset=binary.size();const auto *bytes=static_cast<const unsigned char *>(data);
            binary.insert(binary.end(),bytes,bytes+size);return offset;
        };
        const auto p=append(positions,sizeof positions),n=append(normals,sizeof normals),t=append(uv,sizeof uv),i=append(indices,sizeof indices),image=append(png,sizeof png);
        while(binary.size()%4)binary.push_back(0);
        std::ostringstream json;
        json<<"{\"asset\":{\"version\":\"2.0\"},\"scene\":0,\"scenes\":[{\"nodes\":[0]}],\"nodes\":[{\"mesh\":0}],\"buffers\":[{\"byteLength\":"<<binary.size()<<"}],\"bufferViews\":[";
        const std::pair<std::size_t,std::size_t> views[]={{p,sizeof positions},{n,sizeof normals},{t,sizeof uv},{i,sizeof indices},{image,sizeof png}};
        for(unsigned k=0;k<5;k++){if(k)json<<',';json<<"{\"buffer\":0,\"byteOffset\":"<<views[k].first<<",\"byteLength\":"<<views[k].second<<'}';}
        json<<"],\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\",\"min\":[-0.8,0,0],\"max\":[0.8,1.2,0]},{\"bufferView\":1,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\"},{\"bufferView\":2,\"componentType\":5126,\"count\":3,\"type\":\"VEC2\"},{\"bufferView\":3,\"componentType\":5123,\"count\":3,\"type\":\"SCALAR\"}],\"images\":[{\"bufferView\":4,\"mimeType\":\"image/png\"}],\"textures\":[{\"source\":0}],\"materials\":[{\"alphaMode\":\""<<mode<<"\",\"alphaCutoff\":0.3333,\"pbrMetallicRoughness\":{\"baseColorTexture\":{\"index\":0},\"roughnessFactor\":0.8}}],\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0,\"NORMAL\":1,\"TEXCOORD_0\":2},\"indices\":3,\"material\":0}]}]}";
        std::string text=json.str();while(text.size()%4)text.push_back(' ');
        char path[]="/tmp/valwood-alpha-mode-XXXXXX";const int fd=mkstemp(path);assert(fd>=0);close(fd);
        const uint32_t header[]={0x46546c67,2,uint32_t(28+text.size()+binary.size()),uint32_t(text.size()),0x4e4f534a};
        const uint32_t chunk[]={uint32_t(binary.size()),0x004e4942};
        {std::ofstream file(path,std::ios::binary);assert(file);file.write(reinterpret_cast<const char *>(header),sizeof header);file.write(text.data(),text.size());file.write(reinterpret_cast<const char *>(chunk),sizeof chunk);file.write(reinterpret_cast<const char *>(binary.data()),binary.size());assert(file);}
        WorldModelAsset *asset=world_model_load(path);assert(asset);assert(unlink(path)==0);
        const WorldModelInstance instance={0,0,0,0,1};assert(world_model_upload_instances(asset,&instance,1));
        float lo[3],hi[3];assert(world_model_bounds(asset,lo,hi));
        configure_program(color,lo,hi,white_depth);configure_program(shadow,lo,hi,white_depth);
        const bool opaque=std::strcmp(mode,"OPAQUE")==0;
        check_color(asset,color,opaque);check_shadow(asset,shadow,opaque);
        {WorldModelDepthMaterialScope shadow_scope(shadow);check_color(asset,color,opaque);}
        const float expected=opaque?0.f:std::strcmp(mode,"MASK")==0?.3333f:.4f;
        for(GLuint program:{color,shadow}){float cutoff=-1;glGetUniformfv(program,glGetUniformLocation(program,"uAlphaCutoff"),&cutoff);assert(std::fabs(cutoff-expected)<.000001f);}
        world_model_free(asset);
    }
    assert(texture_cache_stats().entries==0&&glGetError()==GL_NO_ERROR);
    const auto depth_materials=world_model_depth_material_stats();
    assert(depth_materials.opaque_draws>0&&depth_materials.masked_draws>0);
    std::puts("PASS source alpha modes and exact depth-material parity: opaque neutral binding, original cutout coverage, nested restore, unrelated color program unchanged");
}

void check_source_facade_contract(const std::string &root,GLuint color,GLuint shadow,GLuint white_depth){
    std::array<std::array<double,3>,3> colors={};unsigned pass=0;
    for(const char *name:{"wood_zero","wood_worn","plaster"}){
        const std::string path=root+"/tests/fixtures/source_facade/"+name+".glb";
        WorldModelAsset *asset=world_model_load(path.c_str());assert(asset);
        const WorldModelInstance instance={0,0,0,0,1};assert(world_model_upload_instances(asset,&instance,1));
        float lo[3],hi[3];assert(world_model_bounds(asset,lo,hi));configure_program(color,lo,hi,white_depth);configure_program(shadow,lo,hi,white_depth);
        glUseProgram(color);glUniform3f(glGetUniformLocation(color,"uSunColor"),0,0,0);
        glUniform3f(glGetUniformLocation(color,"uSkyLight"),.5,.5,.5);glUniform3f(glGetUniformLocation(color,"uGroundLight"),.5,.5,.5);
        check_color(asset,color);check_shadow(asset,shadow);
        // Red wear paint must change the actual shared source albedo in the
        // real shader; ordinary mode0/opaque alpha behavior is checked above.
        check_color(asset,color);
        std::vector<unsigned char> pixels(target_size*target_size*4);glReadPixels(0,0,target_size,target_size,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
        for(size_t i=0;i<pixels.size();i+=4)for(unsigned c=0;c<3;c++)colors[pass][c]+=pixels[i+c];
        for(GLuint program:{color,shadow}){
            const GLint location=glGetUniformLocation(program,"uFacadeMode");
            if(location>=0){GLint mode=-1;glGetUniformiv(program,location,&mode);assert(mode==0);}
        }
        for(unsigned j=0;j<3;j++){const std::string uniform="uFacadeAux"+std::to_string(j);GLint unit=-1;glGetUniformiv(color,glGetUniformLocation(color,uniform.c_str()),&unit);assert(unit==int(8+j));}
        for(unsigned j=0;j<(pass==2?3u:2u);j++){
            glActiveTexture(GL_TEXTURE8+j);GLint format=0;glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_INTERNAL_FORMAT,&format);
            const bool srgb=pass!=2||j==2;
            assert(srgb?(format==GL_SRGB8_ALPHA8):(format==GL_RGBA||format==GL_RGBA8));
        }
        world_model_free(asset);assert(texture_cache_stats().entries==0);pass++;
    }
    // The real BRDF includes an achromatic dielectric reflection, so its
    // image ratios are less saturated than the raw source texture values.
    assert(colors[0][0]>colors[0][1]*2.5&&colors[1][1]>colors[1][0]*2.5&&colors[2][0]>colors[2][1]*2.5);
    for(const char *name:{"bad_mode","bad_paint","missing_paint","bad_index","bad_scale","bad_version"}){
        const std::string path=root+"/tests/fixtures/source_facade/"+name+".glb";
        assert(!world_model_load(path.c_str()));assert(texture_cache_stats().entries==0);
    }
    assert(glGetError()==GL_NO_ERROR);
    std::puts("PASS source facade contract: source red paint changes actual wood color; plaster stays source-colored; opaque utility alpha retained; mode resets; invalid version/mode/index/scale/paint rejected with no cache leaks");
}

void check_authored_source_facades(const std::string &root,GLuint color,GLuint shadow,GLuint white_depth){
    const char *stage=SDL_getenv("VALWOOD_FACADE_STAGE");
    const std::string directory=stage?stage:root+"/projects/valwood_world/models";
    unsigned source_materials=0;
    for(const char *family:{"house_a","house_b","house_c","house_d","house_e","house_f"}){
        const std::string path=directory+"/purchased_village_"+family+"_near.glb";
        cgltf_options options={};cgltf_data *data=nullptr;assert(cgltf_parse_file(&options,path.c_str(),&data)==cgltf_result_success);
        for(size_t i=0;i<data->materials_count;i++)for(size_t j=0;j<data->materials[i].extensions_count;j++)
            if(!std::strcmp(data->materials[i].extensions[j].name,"VALWOOD_materials_facade"))source_materials++;
        cgltf_free(data);
        WorldModelAsset *asset=world_model_load(path.c_str());assert(asset);
        const WorldModelInstance instance={0,0,0,0,1};assert(world_model_upload_instances(asset,&instance,1));
        float lo[3],hi[3];assert(world_model_bounds(asset,lo,hi));configure_program(color,lo,hi,white_depth);configure_program(shadow,lo,hi,white_depth);
        check_color(asset,color);check_shadow(asset,shadow);
        world_model_free(asset);assert(texture_cache_stats().entries==0);
    }
    assert(source_materials==11&&glGetError()==GL_NO_ERROR);
    std::puts("PASS six actual source-UV near houses: eleven layered source materials render real color/depth; all shared image references released");
}

void check_draw_prefix(WorldModelAsset *asset, GLuint color, GLuint shadow,
                       const float lo[3], const float hi[3], GLuint white_depth) {
    float extent=0;for(int k=0;k<3;++k)extent=std::max(extent,hi[k]-lo[k]);
    const float center_x=(lo[0]+hi[0])*.25f,center_y=(lo[1]+hi[1])*.25f,center_z=(lo[2]+hi[2])*.25f;
    const WorldModelInstance instances[2]={
        {-extent*.38f-center_x,-center_y,-center_z,0,.5f},
        { extent*.38f-center_x,-center_y,-center_z,0,.5f}
    };
    assert(world_model_upload_instances(asset,instances,2));
    struct Sample {GLuint primitives=0;std::vector<unsigned char> covered;};
    GLuint query=0;glGenQueries(1,&query);
    for(GLuint program:{color,shadow}) {
        const bool depth_only=program==shadow;
        glUseProgram(program);glUniform3f(glGetUniformLocation(program,"uOffset"),0,0,0);
        glDrawBuffer(depth_only?GL_NONE:GL_COLOR_ATTACHMENT0);
        glReadBuffer(depth_only?GL_NONE:GL_COLOR_ATTACHMENT0);
        auto draw=[&](size_t count,bool full_wrapper=false) {
            glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
            glBeginQuery(GL_PRIMITIVES_GENERATED,query);
            if(full_wrapper)world_model_draw(asset,program);
            else world_model_draw_instances(asset,program,count);
            glEndQuery(GL_PRIMITIVES_GENERATED);
            Sample sample;glGetQueryObjectuiv(query,GL_QUERY_RESULT,&sample.primitives);
            sample.covered.resize(target_size*target_size);
            if(depth_only) {
                std::vector<float> depth(target_size*target_size);
                glReadPixels(0,0,target_size,target_size,GL_DEPTH_COMPONENT,GL_FLOAT,depth.data());
                for(size_t i=0;i<depth.size();++i){assert(std::isfinite(depth[i]));sample.covered[i]=depth[i]<.999f;}
            } else {
                std::vector<unsigned char> pixels(target_size*target_size*4);
                glReadPixels(0,0,target_size,target_size,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
                for(size_t i=0;i<sample.covered.size();++i)sample.covered[i]=pixels[i*4]||pixels[i*4+1]||pixels[i*4+2];
            }
            assert(world_model_instance_count(asset)==2);assert(glGetError()==GL_NO_ERROR);
            return sample;
        };
        const Sample zero=draw(0),one=draw(1),full=draw(2),oversized=draw(std::numeric_limits<size_t>::max()),wrapped=draw(0,true);
        assert(zero.primitives==0&&std::count(zero.covered.begin(),zero.covered.end(),1)==0);
        assert(one.primitives>0&&full.primitives==2*one.primitives);
        assert(full.primitives==oversized.primitives&&full.primitives==wrapped.primitives);
        assert(full.covered==oversized.covered&&full.covered==wrapped.covered);
        WorldModelInstance invalid[2]={instances[0],instances[1]};invalid[1].scale=std::numeric_limits<float>::quiet_NaN();
        assert(!world_model_upload_instances(asset,invalid,2));
        const Sample preserved=draw(2);
        assert(preserved.primitives==full.primitives&&preserved.covered==full.covered);
        size_t one_left=0,one_right=0,full_left=0,full_right=0;
        for(int y=0;y<target_size;++y)for(int x=0;x<target_size;++x) {
            const size_t i=size_t(y)*target_size+x;
            if(x<target_size/2){one_left+=one.covered[i];full_left+=full.covered[i];assert(one.covered[i]==full.covered[i]);}
            else {one_right+=one.covered[i];full_right+=full.covered[i];}
        }
        assert(one_left>5&&one_right==0&&full_left>5&&full_right>5);
    }
    glUseProgram(0);world_model_draw_instances(asset,color,0);
    GLint current=0;glGetIntegerv(GL_CURRENT_PROGRAM,&current);assert(current==0);
    world_model_draw_instances(asset,0,2);glGetIntegerv(GL_CURRENT_PROGRAM,&current);assert(current==0);
    assert(world_model_instance_count(asset)==2);
    glDeleteQueries(1,&query);glDrawBuffer(GL_COLOR_ATTACHMENT0);glReadBuffer(GL_COLOR_ATTACHMENT0);
    const WorldModelInstance original={0,0,0,0,1};assert(world_model_upload_instances(asset,&original,1));
    configure_program(color,lo,hi,white_depth);configure_program(shadow,lo,hi,white_depth);
    assert(glGetError()==GL_NO_ERROR);
    std::puts("PASS world model instance prefixes: zero/first/full/SIZE_MAX color and shadow pixels/primitives, full-count wrapper and unchanged uploaded count");
}
void check_fading_uploads(WorldModelAsset *asset,GLuint color) {
    const WorldModelInstance original={0,0,0,0,1};
    assert(world_model_upload_instances(asset,&original,1));
    glUseProgram(color);
    const GLint enabled_location=glGetUniformLocation(color,"uInstanceFade");assert(enabled_location>=0);
    auto draw=[&]() {
        glDrawBuffer(GL_COLOR_ATTACHMENT0);glReadBuffer(GL_COLOR_ATTACHMENT0);
        glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        world_model_draw(asset,color);
        std::vector<unsigned char> pixels(target_size*target_size*4);
        glReadPixels(0,0,target_size,target_size,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
        assert(glGetError()==GL_NO_ERROR);return pixels;
    };
    auto area=[](const std::vector<unsigned char> &pixels) {
        size_t count=0;for(size_t i=0;i<pixels.size();i+=4)count+=bool(pixels[i]||pixels[i+1]||pixels[i+2]);return count;
    };
    const auto baseline=draw();const size_t full_area=area(baseline);assert(full_area>50);
    GLint enabled=-1;glGetUniformiv(color,enabled_location,&enabled);assert(enabled==0);
#ifdef WORLD_MODELS_TEST_WRAP_UNIFORM_LOCATION
    const unsigned before=uniform_queries;
#endif
    for(float fade:{0.f,.25f,.5f,.75f,1.f}) {
        assert(world_model_upload_fading_instances(asset,&original,&fade,1));
        const auto pixels=draw();const size_t covered_area=area(pixels);
        assert(std::fabs(float(covered_area)/full_area-fade)<.09f);
        if(fade==0)assert(covered_area==0);
        if(fade==1)assert(pixels==baseline);
        glGetUniformiv(color,enabled_location,&enabled);assert(enabled==1);
    }
    float fade=.5f;assert(world_model_upload_fading_instances(asset,&original,&fade,1));
    const auto half=draw();
    GLuint sentinel=0;glGenBuffers(1,&sentinel);glBindBuffer(GL_ARRAY_BUFFER,sentinel);
    assert(!world_model_upload_fading_instances(asset,&original,nullptr,1));
    assert(!world_model_upload_fading_instances(asset,nullptr,&fade,1));
    assert(!world_model_upload_fading_instances(asset,&original,&fade,4097));
    for(float invalid:{-.01f,1.01f,std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()})
        assert(!world_model_upload_fading_instances(asset,&original,&invalid,1));
    WorldModelInstance invalid=original;invalid.scale=0;
    assert(!world_model_upload_fading_instances(asset,&invalid,&fade,1));
    assert(!world_model_upload_instances(asset,&invalid,1));
    GLint binding=0;glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&binding);assert(GLuint(binding)==sentinel);
    assert(world_model_instance_count(asset)==1&&draw()==half);
    glGetUniformiv(color,enabled_location,&enabled);assert(enabled==1);
    glBindBuffer(GL_ARRAY_BUFFER,0);glDeleteBuffers(1,&sentinel);
    assert(world_model_upload_instances(asset,&original,1));assert(draw()==baseline);
    glGetUniformiv(color,enabled_location,&enabled);assert(enabled==0);
#ifdef WORLD_MODELS_TEST_WRAP_UNIFORM_LOCATION
    assert(uniform_queries==before);
#endif
    std::printf("PASS world model fades: actual attribute6/25/50/75%% coverage over %zu original pixels, exact0/full/reset, invalid fade/instance transaction preserves uploaded pixels/count/bindings, zero repeated uniform queries\n",full_area);
}
}

int main(int argc, char **argv) {
    assert(argc <= 2);
    char resolved[PATH_MAX];
    assert(realpath(argc == 2 ? argv[1] : ".", resolved));
    const std::string root = resolved;
    if (!SDL_getenv("VALWOOD_TEST_NATIVE_GL")) SDL_setenv("SDL_VIDEODRIVER", "offscreen", 1);
    assert(SDL_Init(SDL_INIT_VIDEO) == 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_Window *window = SDL_CreateWindow("World models GL validation", 0, 0, target_size, target_size,
                                          SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    assert(window);
    SDL_GLContext context = SDL_GL_CreateContext(window);
    assert(context);
    std::printf("World models renderer: %s\n", glGetString(GL_RENDERER));

    const std::string vertex = root + "/shaders/world_foliage.vs.glsl";
    const GLuint color = load_program(vertex, root + "/shaders/world_foliage.fs.glsl");
    const GLuint shadow = load_program(vertex, root + "/shaders/world_foliage_shadow.fs.glsl");
    GLuint fbo, color_target, depth_target, white_depth;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glGenTextures(1, &color_target);
    glBindTexture(GL_TEXTURE_2D, color_target);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, target_size, target_size, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color_target, 0);
    glGenTextures(1, &depth_target);
    glBindTexture(GL_TEXTURE_2D, depth_target);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, target_size, target_size, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth_target, 0);
    assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
    glGenTextures(1, &white_depth);
    glBindTexture(GL_TEXTURE_2D, white_depth);
    const float white = 1.f;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, 1, 1, 0, GL_DEPTH_COMPONENT, GL_FLOAT, &white);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glViewport(0, 0, target_size, target_size);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    assert(glGetError() == GL_NO_ERROR);

    assert(!world_model_load((root + "/tests/fixtures/nonexistent-model.glb").c_str()));
    assert(!world_model_upload_instances(nullptr, nullptr, 0));
    assert(!world_model_upload_fading_instances(nullptr,nullptr,nullptr,0));
    assert(!world_model_bounds(nullptr, nullptr, nullptr));
    assert(world_model_instance_count(nullptr) == 0);
    world_model_draw(nullptr, color);
    world_model_draw_instances(nullptr,color,std::numeric_limits<size_t>::max());
    world_model_free(nullptr);
    check_source_alpha_modes(color,shadow,white_depth);
    check_source_facade_contract(root,color,shadow,white_depth);
    check_authored_source_facades(root,color,shadow,white_depth);
    const char *names[] = {
        "silver_fir", "woodland_fern_0", "woodland_fern_1", "woodland_fern_2",
        "mossy_boulder_0", "mossy_boulder_1", "mossy_boulder_2",
        "cinematic_pine_near", "cinematic_pine_mid", "cinematic_pine_far",
        "cinematic_fir_near", "cinematic_fir_mid", "cinematic_fir_far",
        "cinematic_cliff_01", "cinematic_rock_0", "cinematic_rock_1", "cinematic_rock_2",
        "cinematic_grass_0", "cinematic_grass_1", "cinematic_grass_2",
        "cinematic_broadleaf_near", "cinematic_broadleaf_mid", "cinematic_broadleaf_far",
        "cinematic_pine_young_near", "cinematic_pine_young_mid", "cinematic_pine_young_far",
        "cinematic_deadwood", "cinematic_deadwood_mid"
    };
    static_assert(sizeof(names)/sizeof(names[0]) == 28, "21 cinematic and seven legacy fixtures");
    std::vector<WorldModelAsset *> assets;
    std::size_t checked_sampler_textures=0;
    for (const char *name : names) {
        const std::string path = root + "/projects/valwood_world/models/" + name + ".glb";
        const FixtureConstraints *fixture = constraints_for(name);
        const TextureCacheStats previous = texture_cache_stats();
        WorldModelAsset *asset = world_model_load(path.c_str());
        if (!asset) std::fprintf(stderr, "Missing/invalid model fixture: %s (run tools/build_valwood_world.py)\n", path.c_str());
        assert(asset);
        assets.push_back(asset);
        float lo[3], hi[3];
        assert(world_model_bounds(asset, lo, hi));
        assert(lo[1] == 0 && hi[1] > 0);
        for (int i = 0; i < 3; ++i) assert(std::isfinite(lo[i]) && std::isfinite(hi[i]) && hi[i] >= lo[i]);
        check_fixture_source(path, name, fixture, lo, hi);
        if (fixture && fixture->shared_with_previous) {
            const TextureCacheStats current = texture_cache_stats();
            assert(current.entries == previous.entries);
            assert(current.hits >= previous.hits + fixture->images);
            assert(current.references >= previous.references + fixture->images);
        }
        check_instances(asset);
        configure_program(color, lo, hi, white_depth);
        configure_program(shadow, lo, hi, white_depth);
        if(std::string(name).find("_far")!=std::string::npos) {
            // Exercise the runtime's camera-facing impostor and projected-size
            // path, rather than merely drawing the raw card as an opaque prop.
            for(GLuint program:{color,shadow}) {
                glUseProgram(program);
                glUniform1i(glGetUniformLocation(program,"uVegetation"),4);
                glUniform1i(glGetUniformLocation(program,"uLodLevel"),-1);
                glUniform1f(glGetUniformLocation(program,"uPixelScale"),80.f);
                glUniform1f(glGetUniformLocation(program,"uTreeHeight"),fixture ? fixture->tree_height : (std::strstr(name,"fir") ? 22.f : 25.f));
                glUniform3f(glGetUniformLocation(program,"uOffset"),-(lo[0]+hi[0])*.5f,-(lo[1]+hi[1])*.5f,-(hi[1]-lo[1])*.25f);
            }
            if (fixture) {
                const GLint height_location = glGetUniformLocation(color, "uTreeHeight");
                assert(height_location >= 0);
                float assigned = 0;
                glGetUniformfv(color, height_location, &assigned);
                assert(std::fabs(assigned-fixture->tree_height) < .0001f);
                // A 10m or 3.2m tree must disappear below the projected-size
                // threshold; the old hardcoded25m formula would leave pixels.
                glUseProgram(color);
                glUniform1f(glGetUniformLocation(color,"uPixelScale"),.4f);
                check_color(asset, color, false);
                glUniform1f(glGetUniformLocation(color,"uPixelScale"),80.f);
            }
        } else if (fixture && std::strstr(name,"deadwood") == nullptr) {
            for (GLuint program : {color, shadow}) {
                glUseProgram(program);
                glUniform1i(glGetUniformLocation(program,"uVegetation"),1);
            }
        }
        if (assets.size() == 1) {
            check_reusable_uploads(asset);
            check_program_cache(asset, color, shadow);
            check_draw_prefix(asset,color,shadow,lo,hi,white_depth);
            check_fading_uploads(asset,color);
        }
        check_color(asset, color);
        check_shadow(asset, shadow);
        checked_sampler_textures+=check_material_samplers(asset,path,color);
        assert(world_model_upload_instances(asset, nullptr, 0));
        assert(world_model_instance_count(asset) == 0);
        world_model_draw(asset, color);
        world_model_draw(asset, shadow);
        assert(glGetError() == GL_NO_ERROR);
    }
    const TextureCacheStats shared = texture_cache_stats();
    assert(checked_sampler_textures>=28);
    std::printf("Sampler regression: %zu actual material textures preserve declared min/mag/wrap with supported anisotropy bounded at4x PASS\n",checked_sampler_textures);
    assert(shared.entries > 0 && shared.references > 0 && shared.hits > 0);
    for (WorldModelAsset *asset : assets) world_model_free(asset);
    const TextureCacheStats released = texture_cache_stats();
    assert(!released.entries && !released.references && !released.source_bytes);
    assert(glGetError() == GL_NO_ERROR);
    texture_cache_shutdown();
    glDeleteTextures(1, &color_target);
    glDeleteTextures(1, &depth_target);
    glDeleteTextures(1, &white_depth);
    glDeleteFramebuffers(1, &fbo);
    glDeleteProgram(color);
    glDeleteProgram(shadow);
    assert(glGetError() == GL_NO_ERROR);
    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    std::puts("World models GL: 28 real assets (21 cinematic + seven legacy), source bounds/triangles/images, 3.2m/10m billboard fade, color/shadow pixels, invalid instances, alternating programs and shared texture cleanup passed.");
    return 0;
}
