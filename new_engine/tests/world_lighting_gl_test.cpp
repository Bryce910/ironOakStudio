/* Shader invariants, not visual acceptance or a performance benchmark.
 * Default software/offscreen context. VALWOOD_TEST_NATIVE_GL=1 retains the
 * caller's native driver. Run from the engine root or pass it as argv[1].
 */
#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>

namespace {
constexpr int side = 24;
using Pixel = std::array<float, 4>;

std::string read_file(const char *path) {
    std::ifstream input(path);
    assert(input);
    std::ostringstream output; output << input.rdbuf(); return output.str();
}
GLuint shader(GLenum type, const std::string &source) {
    GLuint result = glCreateShader(type);
    const char *text = source.c_str();
    glShaderSource(result, 1, &text, nullptr); glCompileShader(result);
    GLint okay = 0; glGetShaderiv(result, GL_COMPILE_STATUS, &okay);
    if (!okay) {
        char log[8192]; glGetShaderInfoLog(result, sizeof(log), nullptr, log);
        std::fprintf(stderr, "%s\n", log);
    }
    assert(okay); return result;
}
GLuint program(const std::string &vertex, const std::string &fragment) {
    GLuint vs = shader(GL_VERTEX_SHADER, vertex), fs = shader(GL_FRAGMENT_SHADER, fragment);
    GLuint result = glCreateProgram();
    glAttachShader(result, vs); glAttachShader(result, fs); glLinkProgram(result);
    GLint okay = 0; glGetProgramiv(result, GL_LINK_STATUS, &okay);
    if (!okay) {
        char log[8192]; glGetProgramInfoLog(result, sizeof(log), nullptr, log);
        std::fprintf(stderr, "%s\n", log);
    }
    assert(okay); glDeleteShader(vs); glDeleteShader(fs); return result;
}
void scalar(GLuint p, const char *name, float value) {
    glUniform1f(glGetUniformLocation(p, name), value);
}
void vector(GLuint p, const char *name, float x, float y, float z) {
    glUniform3f(glGetUniformLocation(p, name), x, y, z);
}
void sampler(GLuint p, const char *name, int unit) {
    glUniform1i(glGetUniformLocation(p, name), unit);
}
Pixel render(GLuint p) {
    glUseProgram(p); glDrawArrays(GL_TRIANGLES, 0, 3);
    std::vector<Pixel> pixels(side * side);
    glReadPixels(0, 0, side, side, GL_RGBA, GL_FLOAT, pixels.data());
    for (const auto &pixel : pixels) for (float value : pixel) assert(std::isfinite(value));
    assert(glGetError() == GL_NO_ERROR);
    return pixels[(side / 2) * side + side / 2];
}
float energy(const Pixel &p) { return p[0] + p[1] + p[2]; }
GLuint texture(int unit, const Pixel &pixel) {
    GLuint result; glGenTextures(1, &result);
    glActiveTexture(GL_TEXTURE0 + unit); glBindTexture(GL_TEXTURE_2D, result);
    int size = unit == 6 ? 9 : 1;
    std::vector<Pixel> pixels(size * size, pixel);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, size, size, 0, GL_RGBA, GL_FLOAT, pixels.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return result;
}
void height_texture(GLuint id, float height) {
    glActiveTexture(GL_TEXTURE11); glBindTexture(GL_TEXTURE_2D, id);
    std::array<float, 81> values; values.fill(height);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, 9, 9, 0, GL_RED, GL_FLOAT, values.data());
}
void inclined_height_texture(GLuint id) {
    std::array<float, 81> values;
    for (int z = 0; z < 9; ++z) for (int x = 0; x < 9; ++x)
        values[z * 9 + x] = 300.f + (x / 8.f - .5f) * 100800.f;
    glActiveTexture(GL_TEXTURE11); glBindTexture(GL_TEXTURE_2D, id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, 9, 9, 0, GL_RED, GL_FLOAT, values.data());
}
void solid_texture(GLuint id, int unit, const Pixel &pixel, int size = 1) {
    glActiveTexture(GL_TEXTURE0 + unit); glBindTexture(GL_TEXTURE_2D, id);
    std::vector<Pixel> values(size * size, pixel);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, size, size, 0, GL_RGBA, GL_FLOAT, values.data());
}
const char *controlled_vertex = R"GLSL(#version 330 core
uniform vec3 tPosition, tNormal, tRelative;
uniform vec2 tWorldSpan;
out vec3 vPos, vNormal, vRelative;
void main() {
    vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);
    gl_Position=vec4(p*2.-1.,0,1);
    vPos=tPosition+vec3((p.x-.5)*tWorldSpan.x,0,(p.y-.5)*tWorldSpan.y);
    vNormal=tNormal;vRelative=tRelative;
})GLSL";

void terrain_tests(GLuint land, GLuint macro_height, GLuint sand, GLuint climate) {
    glUseProgram(land);
    const char *names[] = {"uGrass", "uRock", "uSand", "uDirt", "uGrassNormal", "uRockNormal",
        "uClimate", "uShadow", "uGrassARM", "uRockARM"};
    for (int i = 0; i < 10; ++i) sampler(land, names[i], i);
    sampler(land, "uVillagePBR", 10); sampler(land, "uMacroHeight", 11); sampler(land, "uLandscape", 12);
    sampler(land, "uLitterPBR", 13); sampler(land, "uVillageGroundMask", 14);
    glUniform2f(glGetUniformLocation(land, "uWorldSize"), 100800, 56708.804f);
    const float districts[12] = {1e6f, 1e6f, 1, 1, 1e6f, 1e6f, 1, 1, 1e6f, 1e6f, 1, 1};
    glUniform4fv(glGetUniformLocation(land, "uDistricts"), 3, districts);
    glUniform4f(glGetUniformLocation(land, "uTrailBounds"), 1e6f, 1e6f, 1e6f, 1e6f);
    const float light_matrix[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
    glUniformMatrix4fv(glGetUniformLocation(land, "uLightVP"), 1, GL_FALSE, light_matrix);
    vector(land, "tPosition", 0, 300, 0); vector(land, "tNormal", 0, 1, 0);
    vector(land, "tRelative", 0, -2, -5); vector(land, "uSun", .6f, .8f, 0);
    vector(land, "uSunColor", 0, 0, 0); vector(land, "uAmbient", 0, 0, 0);
    vector(land, "uSkyLight", .05f, .1f, .8f); vector(land, "uGroundLight", .8f, .1f, .02f);
    Pixel up = render(land);
    vector(land, "tNormal", 0, -1, 0); Pixel down = render(land);
    assert(up[2] > up[0] * 2 && down[0] > down[2] * 2);

    vector(land, "tNormal", 0, 1, 0);
    vector(land, "uSkyLight", .3f, .3f, .3f); vector(land, "uGroundLight", .08f, .08f, .08f);
    Pixel open = render(land);
    height_texture(macro_height, 1200); Pixel valley = render(land);
    float sky_ratio = energy(valley) / energy(open);
    assert(sky_ratio < .65f && sky_ratio > .30f);
    // An object above its surroundings does not acquire a valley's occlusion.
    vector(land, "tPosition", 0, 1300, 0); Pixel exposed = render(land);
    height_texture(macro_height, 0); Pixel exposed_open = render(land);
    assert(std::abs(energy(exposed) - energy(exposed_open)) < 1e-5f);

    // A continuous 45-degree hillside changes the hemisphere, but adds no
    // obstruction beyond its own tangent. The actual filtered height texture
    // spans the full map; this catches double darkening by naive raw horizons.
    vector(land, "tPosition", 0, 300, 0);
    vector(land, "tNormal", -.70710678f, .70710678f, 0);
    height_texture(macro_height, -10000); Pixel open_slope = render(land);
    inclined_height_texture(macro_height); Pixel planar_slope = render(land);
    assert(std::abs(energy(open_slope) - energy(planar_slope)) < 1e-4f);
    vector(land, "tNormal", 0, 1, 0); height_texture(macro_height, 0);

    vector(land, "tPosition", 0, 300, 0);
    vector(land, "uSkyLight", 0, 0, 0); vector(land, "uGroundLight", 0, 0, 0);
    vector(land, "uAmbient", .2f, .2f, .2f); Pixel legacy = render(land);
    vector(land, "uSkyLight", .25f, .276f, .304f);
    vector(land, "uGroundLight", .096f, .1f, .086f); Pixel explicit_legacy = render(land);
    for (int i = 0; i < 3; ++i) assert(std::abs(legacy[i] - explicit_legacy[i]) < 2e-5f);
    vector(land, "uSkyLight", 0, 0, 0); vector(land, "uGroundLight", 0, 0, 0);
    vector(land, "uAmbient", 0, 0, 0); assert(energy(render(land)) < 1e-6f);

    vector(land, "uSunColor", 1, 1, 1); Pixel direct = render(land);
    height_texture(macro_height, 1200); Pixel shadowed = render(land);
    assert(energy(shadowed) < energy(direct) * .25f);
    height_texture(macro_height, 0);
    float min_transmission = 1, max_transmission = 0;
    for (int z = 0; z < 8; ++z) for (int x = 0; x < 8; ++x) {
        vector(land, "tPosition", (x - 4) * 1500.f, 300, (z - 4) * 1500.f);
        scalar(land, "uCloud", 0); Pixel clear = render(land);
        scalar(land, "uCloud", 1); Pixel cloudy = render(land);
        float ratio = energy(cloudy) / energy(clear);
        assert(ratio >= .659f && ratio <= 1.001f);
        min_transmission = std::min(min_transmission, ratio);
        max_transmission = std::max(max_transmission, ratio);
    }
    assert(min_transmission < .80f && max_transmission > .98f);
    std::printf("Lighting: horizon ratio %.4f; bounded cloud transmission %.4f..%.4f\n",
        sky_ratio, min_transmission, max_transmission);

    // Low inland ground must neither select the coastal gravel nor acquire a
    // waterline wet band. Vary actual input textures and elevation rather than
    // duplicating the shader's material-mask formulas in the test.
    scalar(land, "uCloud", 0); vector(land, "uSunColor", 0, 0, 0);
    vector(land, "uSkyLight", .3f, .3f, .3f); vector(land, "uGroundLight", .08f, .08f, .08f);
    vector(land, "tPosition", 1300, .2f, -800);
    solid_texture(sand, 2, {1, .05f, .02f, 1}); Pixel inland_red = render(land);
    solid_texture(sand, 2, {.02f, .05f, 1, 1}); Pixel inland_blue = render(land);
    for (int i = 0; i < 3; ++i) assert(std::abs(inland_red[i] - inland_blue[i]) < 1e-6f);
    vector(land, "tPosition", 1300, 5, -800); Pixel inland_high = render(land);
    for (int i = 0; i < 3; ++i) assert(std::abs(inland_blue[i] - inland_high[i]) < 1e-5f);

    solid_texture(climate, 6, {0, .5f, 0, .5f}, 9);
    vector(land, "tPosition", 1300, .2f, -800); Pixel coast_blue = render(land);
    solid_texture(sand, 2, {1, .05f, .02f, 1}); Pixel coast_red = render(land);
    assert(std::abs(coast_red[0] - coast_blue[0]) > .01f);
    assert(std::abs(coast_red[2] - coast_blue[2]) > .01f);
    solid_texture(sand, 2, {.4f, .4f, .4f, 1});
    float max_step = 0, max_energy = 0;
    Pixel previous = render(land);
    for (int step = 0; step <= 320; ++step) {
        vector(land, "tPosition", 1300, step * .025f, -800);
        Pixel current = render(land);
        max_energy = std::max(max_energy, energy(current));
        if (step > 0) max_step = std::max(max_step, std::abs(energy(current) - energy(previous)));
        previous = current;
    }
    assert(max_step < max_energy * .04f);
    std::printf("Lighting: inland dry/gravel exclusion; shore max25mm step %.4f%% PASS\n", 100 * max_step / max_energy);
}

void rainforest_tests(GLuint land, GLuint climate,GLuint dirt) {
    glUseProgram(land);
    solid_texture(climate, 6, {.60f, .94f, 0, 1}, 9);
    vector(land, "tNormal", 0, 1, 0); vector(land, "tRelative", 0, -2, -5);
    vector(land, "uSunColor", 0, 0, 0); scalar(land, "uCloud", 0);
    vector(land, "uSkyLight", .3f, .3f, .3f); vector(land, "uGroundLight", .08f, .08f, .08f);
    constexpr float cx=-26712.f, cz=9640.49668f;
    glUniform4f(glGetUniformLocation(land, "uRainforestBounds"), cx, cz, 1.f/6552.f, 1.f/4820.24834f);
    const float path[12]={cx-15,cz+95,cx+2,cz+60,cx-8,cz+20,cx+5,cz-25,cx-5,cz-65,cx+12,cz-110};
    glUniform2fv(glGetUniformLocation(land, "uRainforestPath"), 6, path);
    float outside_error=0;
    for(int i=0;i<24;++i) {
        vector(land, "tPosition", cx+7000.f+i*137.f, 80.f+i*30.f, cz+(i-12)*219.f);
        scalar(land, "uRainforest", 0); scalar(land,"uWetness",0); Pixel disabled=render(land);
        scalar(land, "uRainforest", 1); scalar(land,"uWetness",1); Pixel enabled=render(land);
        for(int k=0;k<4;++k)outside_error=std::max(outside_error,std::abs(disabled[k]-enabled[k]));
    }
    assert(outside_error==0);
    scalar(land,"uWetness",0);
    vector(land, "tPosition", cx+50, 120, cz+50);
    scalar(land, "uRainforest", 0); Pixel dry=render(land);
    scalar(land, "uRainforest", 1); Pixel wet=render(land);
    float material_delta=std::abs(energy(dry)-energy(wet));assert(material_delta>.005f);
    scalar(land,"uWetness",1);Pixel storm=render(land);
    assert(energy(storm)<energy(wet)-.001f&&energy(storm)>energy(wet)*.90f&&storm[3]==wet[3]);
    scalar(land,"uWetness",0);
    vector(land, "uEye", 40000, 3000, -20000); Pixel distant_observer=render(land);
    for(int k=0;k<4;++k)assert(wet[k]==distant_observer[k]);
    vector(land, "tPosition", cx+6552.f*std::sqrt(.9999f), 120, cz);
    Pixel edge=render(land);scalar(land, "uRainforest", 0);Pixel edge_disabled=render(land);
    for(int k=0;k<4;++k)assert(std::abs(edge[k]-edge_disabled[k])<1e-5f);
    // A source path point must select the photographic earth, and changing
    // the uploaded path must remove that selection without moving the biome.
    // Give soil a contrasting map for this material-selection assertion;
    // equal grey maps can have matching luminance at different blend weights.
    solid_texture(dirt,3,{.08f,.035f,.015f,1});
    scalar(land, "uRainforest", 1);vector(land, "tPosition", cx+2, 120, cz+60);Pixel on_path=render(land);
    float remote[12];for(int i=0;i<12;++i)remote[i]=path[i]+10000.f;
    glUniform2fv(glGetUniformLocation(land, "uRainforestPath"), 6, remote);Pixel without_path=render(land);
    float path_delta=std::abs(energy(on_path)-energy(without_path));assert(path_delta>.005f);
    glUniform2fv(glGetUniformLocation(land, "uRainforestPath"), 6, path);scalar(land, "uRainforest", 0);
    std::printf("Lighting: localized rainforest outside exact %.8g, material %.6f, shared path %.6f, smooth edge/camera independence PASS\n",outside_error,material_delta,path_delta);
    glUniform1i(glGetUniformLocation(land,"uClipBelowWater"),1);
    scalar(land,"uWaterClipLevel",134);
    auto clipped=[&](float height){vector(land,"tPosition",cx+50,height,cz+50);glClearColor(0,0,0,0);glClear(GL_COLOR_BUFFER_BIT);return render(land);};
    assert(clipped(133.99f)[3]==0&&clipped(134.01f)[3]==1);
    scalar(land,"uWaterClipLevel",0);assert(clipped(.19f)[3]==0&&clipped(.21f)[3]==1);
    glUniform1i(glGetUniformLocation(land,"uClipBelowWater"),0);
    solid_texture(dirt,3,{.4f,.4f,.4f,1});
    std::puts("Lighting: raised-pool reflection clips real134m plane; default ocean20cm guard preserved PASS");
}

void freshwater_bank_tests(GLuint land,GLuint sand,GLuint climate) {
    glUseProgram(land);sampler(land,"uHasVillage",0);scalar(land,"uRainforest",0);
    vector(land,"tNormal",0,1,0);vector(land,"tRelative",0,-2,-5);
    vector(land,"uSunColor",0,0,0);scalar(land,"uCloud",0);scalar(land,"uWetness",0);
    vector(land,"uSkyLight",.3f,.3f,.3f);vector(land,"uGroundLight",.08f,.08f,.08f);
    solid_texture(climate,6,{.55f,.9f,0,1},9);
    auto pool=[&](bool enabled){glUniform4f(glGetUniformLocation(land,"uFreshwaterPool"),1300,-800,enabled?.1f:0,enabled?1.f/16.f:0);};
    scalar(land,"uFreshwaterLevel",134);pool(true);vector(land,"tPosition",1300,134.1f,-800);
    solid_texture(sand,2,{1,.05f,.02f,1});const Pixel red=render(land);
    solid_texture(sand,2,{.02f,.05f,1,1});const Pixel blue=render(land);
    assert(std::abs(red[0]-blue[0])>.005f&&std::abs(red[2]-blue[2])>.005f);
    // Opposite source textures affect real shallow sediment. They cannot
    // paint a bank above its wet reach or beyond the conservative local cap.
    for(const auto &point:std::array<std::array<float,3>,5>{{{1330,134.1f,-800},{1300,134.1f,-830},
            {1300,135.1f,-800},{1400,133.5f,-700},{1300,300,-800}}}) {
        vector(land,"tPosition",point[0],point[1],point[2]);pool(false);const Pixel disabled=render(land);
        pool(true);assert(disabled==render(land));
    }
    solid_texture(sand,2,{.4f,.4f,.4f,1});float max_step=0,max_energy=0;
    vector(land,"tPosition",1300,133.7f,-800);Pixel previous=render(land);
    for(int step=0;step<=140;step++) {
        vector(land,"tPosition",1300,133.7f+step*.01f,-800);const Pixel current=render(land);
        max_energy=std::max(max_energy,energy(current));
        max_step=std::max(max_step,std::abs(energy(current)-energy(previous)));previous=current;
    }
    assert(max_step<max_energy*.025f);
    vector(land,"tPosition",1300,134.1f,-800);const Pixel anchored=render(land);
    scalar(land,"uTime",1000000);vector(land,"uEye",-40000,3000,25000);assert(anchored==render(land));scalar(land,"uTime",0);
    pool(false);sampler(land,"uHasVillage",1);glUniform4f(glGetUniformLocation(land,"uVillageBounds"),-280,-280,1.f/560,1.f/560);
    vector(land,"tPosition",0,22.95f,0);solid_texture(sand,2,{1,.05f,.02f,1});const Pixel courtyard_red=render(land);
    solid_texture(sand,2,{.02f,.05f,1,1});assert(courtyard_red==render(land));
    vector(land,"tPosition",219,22.95f,0);const Pixel moat_blue=render(land);
    solid_texture(sand,2,{1,.05f,.02f,1});const Pixel moat_red=render(land);
    assert(std::abs(moat_red[0]-moat_blue[0])>.005f);
    vector(land,"tPosition",219,24,0);const Pixel dry_exterior_red=render(land);
    solid_texture(sand,2,{.02f,.05f,1,1});assert(dry_exterior_red==render(land));
    solid_texture(sand,2,{.4f,.4f,.4f,1});sampler(land,"uHasVillage",0);
    std::printf("Lighting: freshwater uses actual sediment textures, remote/high/courtyard exact unchanged; moat exterior only; max1cm bank change %.4f%%; world/time anchored PASS\n",max_step/max_energy*100);
}

void local_irradiance_tests(GLuint land) {
    glUseProgram(land);vector(land,"tPosition",1300,300,-800);vector(land,"tNormal",0,1,0);
    vector(land,"uSunColor",0,0,0);scalar(land,"uCloud",0);scalar(land,"uRainforest",0);
    glUniform4f(glGetUniformLocation(land,"uLocalIrradianceBounds"),1200,-900,1400,-700);
    glUniform2f(glGetUniformLocation(land,"uLocalIrradianceHeights"),200,400);
    scalar(land,"uLocalIrradianceWeight",0);const Pixel legacy=render(land);
    std::array<float,27> coefficients={};coefficients[0]=coefficients[1]=coefficients[2]=.2f/.282095f;
    glUniform3fv(glGetUniformLocation(land,"uLocalIrradiance[0]"),9,coefficients.data());
    assert(legacy==render(land));scalar(land,"uLocalIrradianceWeight",1);const Pixel captured=render(land);
    assert(energy(captured)>.001f&&energy(captured)<energy(legacy));
    scalar(land,"uLocalIrradianceWeight",.5f);const Pixel half=render(land);
    for(int c=0;c<3;c++)assert(std::abs(half[c]-(captured[c]+legacy[c])*.5f)<1e-6f);
    coefficients[0]=coefficients[1]=coefficients[2]=-.1f;glUniform3fv(glGetUniformLocation(land,"uLocalIrradiance[0]"),9,coefficients.data());
    scalar(land,"uLocalIrradianceWeight",1);assert(energy(render(land))==0);
    // SH coefficients are world-space irradiance, unaffected by eye/time.
    coefficients[0]=coefficients[1]=coefficients[2]=.2f/.282095f;
    coefficients[3]=.08f;coefficients[4]=.04f;coefficients[5]=-.03f;
    glUniform3fv(glGetUniformLocation(land,"uLocalIrradiance[0]"),9,coefficients.data());const Pixel directional=render(land);
    assert(std::abs(energy(directional)-energy(captured))>.0001f);
    scalar(land,"uTime",1000000);vector(land,"uEye",40000,-500,-25000);assert(directional==render(land));
    scalar(land,"uTime",0);scalar(land,"uLocalIrradianceWeight",0);assert(legacy==render(land));
    for(const auto &point:std::array<std::array<float,3>,5>{{{1200,300,-800},{1199,300,-800},
            {1400,300,-800},{1300,200,-800},{1300,400,-800}}}) {
        vector(land,"tPosition",point[0],point[1],point[2]);scalar(land,"uLocalIrradianceWeight",0);const Pixel outside=render(land);
        scalar(land,"uLocalIrradianceWeight",1);assert(outside==render(land));
    }
    scalar(land,"uLocalIrradianceWeight",0);
    std::puts("Lighting: captured SH9 weight0 exact, bounded replacement/half-weight linearity, negative lobes nonnegative, directional response/world-time invariance PASS");
}

void village_layer_tests(GLuint land, GLuint landscape, GLuint climate) {
    glUseProgram(land);
    solid_texture(climate,6,{0,.5f,0,1},9);
    vector(land,"tPosition",0,300,0);vector(land,"tNormal",0,1,0);
    vector(land,"tRelative",0,-2,-5);vector(land,"uSunColor",0,0,0);
    vector(land,"uSkyLight",.3f,.3f,.3f);vector(land,"uGroundLight",.08f,.08f,.08f);
    glUniform4f(glGetUniformLocation(land,"uVillageBounds"),-100,-100,.005f,.005f);
    glUniform1i(glGetUniformLocation(land,"uHasVillage"),0);const Pixel disabled=render(land);
    glUniform1i(glGetUniformLocation(land,"uHasVillage"),1);const Pixel empty=render(land);
    for(int k=0;k<4;++k)assert(disabled[k]==empty[k]);
    glActiveTexture(GL_TEXTURE12);glBindTexture(GL_TEXTURE_2D_ARRAY,landscape);
    GLint layers=0;glGetTexLevelParameteriv(GL_TEXTURE_2D_ARRAY,0,GL_TEXTURE_DEPTH,&layers);assert(layers==4);
    const Pixel paving={0,1,0,1};
    glTexSubImage3D(GL_TEXTURE_2D_ARRAY,0,0,0,3,1,1,1,GL_RGBA,GL_FLOAT,paving.data());
    const Pixel village=render(land);float difference=0;
    for(int k=0;k<3;++k)difference+=std::abs(disabled[k]-village[k]);
    assert(difference>.005f);
    // The fourth layer only applies inside its authored bounds; the same
    // sample remains byte-identical when the bounds move away from it.
    glUniform4f(glGetUniformLocation(land,"uVillageBounds"),1000,1000,.005f,.005f);
    const Pixel outside=render(land);
    for(int k=0;k<4;++k)assert(disabled[k]==outside[k]);
    glUniform1i(glGetUniformLocation(land,"uHasVillage"),0);const Pixel reset=render(land);
    for(int k=0;k<4;++k)assert(disabled[k]==reset[k]);
    std::printf("Lighting: actual fourth landscape layer changes local paving %.6f; empty/outside/disabled exact PASS\n",difference);
}

void ground_surface_tests(GLuint land, GLuint landscape, GLuint climate, GLuint grass, GLuint earth) {
    glUseProgram(land);
    // Constant scan inputs isolate the ground's connected material patches.
    // Exercise the actual market coordinates, far from the floating origin.
    solid_texture(climate,6,{0,.5f,0,1},9);
    solid_texture(grass,0,{.4f,.4f,.4f,1});
    solid_texture(earth,3,{.4f,.4f,.4f,1});
    vector(land,"tNormal",0,1,0);vector(land,"tRelative",0,-2,-5);
    vector(land,"uSunColor",0,0,0);scalar(land,"uCloud",0);scalar(land,"uHaze",0);
    vector(land,"uSkyLight",.3f,.3f,.3f);vector(land,"uGroundLight",.08f,.08f,.08f);
    constexpr float cx=-1310.4f,cz=-10207.585f;
    glUniform4f(glGetUniformLocation(land,"uVillageBounds"),cx-128,cz-128,1.f/256,1.f/256);
    glUniform1i(glGetUniformLocation(land,"uHasVillage"),1);
    glActiveTexture(GL_TEXTURE12);glBindTexture(GL_TEXTURE_2D_ARRAY,landscape);
    const Pixel paving={0,1,0,1};
    glTexSubImage3D(GL_TEXTURE_2D_ARRAY,0,0,0,3,1,1,1,GL_RGBA,GL_FLOAT,paving.data());
    float minimum=10,maximum=0;
    for(int z=-8;z<=8;++z)for(int x=-8;x<=8;++x) {
        vector(land,"tPosition",cx+x*4,24,cz+z*4);
        const Pixel sample=render(land);
        minimum=std::min(minimum,energy(sample));maximum=std::max(maximum,energy(sample));
    }
    const float contrast=maximum/minimum;
    assert(contrast>1.30f&&contrast<2.2f);
    // Smooth walking through a patch must not reveal hard cells or flickering
    // temporal noise. Vary time and the camera independently at the same point.
    vector(land,"tPosition",cx,24,cz);scalar(land,"uTime",0);
    const Pixel fixed=render(land);
    scalar(land,"uTime",1e6f);vector(land,"uEye",45000,800,-25000);
    vector(land,"tRelative",.025f,-2,-5);
    const Pixel later=render(land);
    for(int k=0;k<4;++k)assert(fixed[k]==later[k]);
    float max_step=0;
    Pixel previous=render(land);
    for(int step=1;step<=400;++step) {
        vector(land,"tPosition",cx+step*.025f,24,cz);
        const Pixel current=render(land);
        max_step=std::max(max_step,std::abs(energy(current)-energy(previous)));
        previous=current;
    }
    assert(max_step<maximum*.012f);
    glUniform1i(glGetUniformLocation(land,"uHasVillage"),0);
    vector(land,"tPosition",cx,24,cz);const Pixel meadow=render(land);
    glUniform1i(glGetUniformLocation(land,"uHasVillage"),1);const Pixel road=render(land);
    float material_delta=0;
    for(int k=0;k<3;++k)material_delta+=std::abs(meadow[k]-road[k]);
    assert(material_delta>.02f);
    glUniform1i(glGetUniformLocation(land,"uHasVillage"),0);scalar(land,"uTime",0);
    std::printf("Lighting: connected ground patch contrast %.3f; 25mm max step %.4f%%; time/camera exact; paving/meadow %.6f PASS\n",
        contrast,100*max_step/maximum,material_delta);
}

void village_ground_history_tests(GLuint land,GLuint landscape,GLuint pbr,GLuint litter,GLuint history,GLuint climate,GLuint dirt) {
    glUseProgram(land);sampler(land,"uLitterPBR",13);sampler(land,"uVillageGroundMask",14);
    constexpr float cx=-1310.4f,cz=-10207.585f;
    vector(land,"tPosition",cx,24,cz);vector(land,"tNormal",0,1,0);vector(land,"tRelative",0,-3,0);
    solid_texture(climate,6,{0,.5f,0,1},9);solid_texture(dirt,3,{.07f,.035f,.015f,1});
    vector(land,"uSun",0,1,0);vector(land,"uSunColor",0,0,0);vector(land,"uAmbient",0,0,0);vector(land,"uSkyLight",.3f,.3f,.3f);vector(land,"uGroundLight",0,0,0);
    scalar(land,"uCloud",0);scalar(land,"uHaze",0);scalar(land,"uWetness",0);
    sampler(land,"uHasStarting",0);sampler(land,"uHasVillage",1);sampler(land,"uVillagePBRReady",1);sampler(land,"uVillageSkyEnabled",0);sampler(land,"uVillageGroundReady",1);
    glUniform4f(glGetUniformLocation(land,"uVillageBounds"),cx-128,cz-128,1.f/256,1.f/256);
    glUniform4f(glGetUniformLocation(land,"uVillageGroundBounds"),cx-90,cz-70,1.f/180,1.f/180);
    const auto array_pixel=[](GLuint texture,int unit,int layer,Pixel value){glActiveTexture(GL_TEXTURE0+unit);glBindTexture(GL_TEXTURE_2D_ARRAY,texture);glTexSubImage3D(GL_TEXTURE_2D_ARRAY,0,0,0,layer,1,1,1,GL_RGBA,GL_FLOAT,value.data());};
    const auto mask=[&](Pixel value){solid_texture(history,14,value);};
    const auto equal=[](const Pixel&a,const Pixel&b){for(int i=0;i<4;i++)assert(std::abs(a[i]-b[i])<2e-7f);};
    array_pixel(landscape,12,3,{0,1,0,1});array_pixel(pbr,10,0,{.5f,.5f,.5f,1});array_pixel(pbr,10,1,{.5f,.5f,1,1});array_pixel(pbr,10,2,{.9f,.8f,0,1});
    array_pixel(litter,13,0,{.5f,.5f,1,1});array_pixel(litter,13,1,{1,.8f,0,1});
    mask({0,0,0,0});const Pixel clean=render(land);
    mask({1,0,0,0});const Pixel soil=render(land);assert(energy(soil)<energy(clean)*.94f&&soil[0]/soil[1]>clean[0]/clean[1]);
    mask({0,1,0,0});const Pixel traffic=render(land);assert(energy(traffic)>energy(clean)*1.02f);
    mask({0,0,1,0});const Pixel damp=render(land);assert(energy(damp)<energy(clean)*.97f);
    mask({0,0,0,1});assert(energy(render(land))<energy(clean)*.95f);
    // Missing/disabled/outside/elevated fields preserve the source material.
    sampler(land,"uVillageGroundReady",0);equal(clean,render(land));sampler(land,"uVillageGroundReady",1);
    glUniform4f(glGetUniformLocation(land,"uVillageGroundBounds"),1e6f,1e6f,1.f/180,1.f/180);equal(clean,render(land));
    glUniform4f(glGetUniformLocation(land,"uVillageGroundBounds"),cx-90,cz-70,1.f/180,1.f/180);
    sampler(land,"uHasVillage",0);const Pixel exterior=render(land);mask({1,1,1,1});equal(exterior,render(land));sampler(land,"uHasVillage",1);
    vector(land,"tPosition",cx,43,cz);sampler(land,"uVillageGroundReady",0);const Pixel elevated=render(land);sampler(land,"uVillageGroundReady",1);equal(elevated,render(land));vector(land,"tPosition",cx,24,cz);
    sampler(land,"uVillagePBRReady",0);const Pixel legacy=render(land);mask({0,0,0,0});equal(legacy,render(land));sampler(land,"uVillagePBRReady",1);
    mask({.5f,.4f,.7f,.2f});scalar(land,"uTime",0);const Pixel anchored=render(land);scalar(land,"uTime",1e6f);vector(land,"uEye",45000,800,-25000);equal(anchored,render(land));
    // Black albedo isolates specular: local damp must actually lower roughness,
    // not merely paint the ground darker. Metallic B remains dielectric.
    vector(land,"uSkyLight",0,0,0);vector(land,"uSunColor",1,1,1);array_pixel(pbr,10,0,{0,0,0,1});solid_texture(dirt,3,{0,0,0,1});
    mask({0,0,0,0});const Pixel dry_specular=render(land);mask({0,0,1,0});const Pixel damp_specular=render(land);assert(energy(damp_specular)>energy(dry_specular)*1.15f);
    mask({1,0,0,0});array_pixel(litter,13,1,{1,.72f,0,1});const Pixel soil_smooth=render(land);array_pixel(litter,13,1,{1,.98f,0,1});const Pixel soil_rough=render(land);assert(std::abs(energy(soil_smooth)-energy(soil_rough))>.0001f);
    array_pixel(litter,13,1,{0,.98f,1,1});equal(soil_rough,render(land));
    // Layer0 is the real soil normal: deposits change normal response, while
    // clean paving remains independent of this material's normal/roughness.
    array_pixel(litter,13,0,{.8f,.5f,.9f,1});assert(std::abs(energy(render(land))-energy(soil_rough))>.0001f);
    mask({0,0,0,0});const Pixel clean_normal=render(land);array_pixel(litter,13,0,{.5f,.5f,1,1});array_pixel(litter,13,1,{1,.8f,0,1});equal(clean_normal,render(land));
    sampler(land,"uVillageGroundReady",0);sampler(land,"uHasVillage",0);scalar(land,"uTime",0);
    std::printf("Lighting: authored ground history channels alter real soil/normal/roughness; dielectric ARM packing, world/time stability, disabled/outside/height exact PASS\n");
}

void village_stochastic_paving_tests(GLuint land,GLuint landscape,GLuint pbr,GLuint history,GLuint climate) {
    glUseProgram(land);
    constexpr int size=8;
    using Layers=std::array<Pixel,size*size*3>;
    Layers source;
    for(int y=0;y<size;y++)for(int x=0;x<size;x++){
        const float a=std::sin((x+.2f)*.83f),b=std::cos((y+.4f)*.91f),c=std::sin((x+y+.7f)*.71f);
        source[y*size+x]={.40f+.18f*a,.37f+.16f*b,.34f+.13f*c,1};
        const float nx=.30f*a,ny=.28f*b;
        source[size*size+y*size+x]={.5f+nx*.5f,.5f+ny*.5f,.5f+std::sqrt(1-nx*nx-ny*ny)*.5f,1};
        source[2*size*size+y*size+x]={.73f+.20f*c,.65f+.20f*a,0,1};
    }
    auto put=[&](const Layers&pixels,bool mips=false){
        glActiveTexture(GL_TEXTURE10);glBindTexture(GL_TEXTURE_2D_ARRAY,pbr);
        glTexImage3D(GL_TEXTURE_2D_ARRAY,0,GL_RGBA32F,size,size,3,0,GL_RGBA,GL_FLOAT,pixels.data());
        glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_WRAP_S,GL_REPEAT);glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_WRAP_T,GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MIN_FILTER,mips?GL_LINEAR_MIPMAP_LINEAR:GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        if(mips)glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
    };
    auto constant=[&](const std::array<Pixel,3>&pixels){Layers result;for(int l=0;l<3;l++)std::fill_n(result.begin()+l*size*size,size*size,pixels[l]);return result;};
    const auto cpu_sample=[&](int layer,float u,float v){
        const float px=(u-std::floor(u))*size-.5f,py=(v-std::floor(v))*size-.5f;
        const int ix=int(std::floor(px)),iy=int(std::floor(py));const float tx=px-std::floor(px),ty=py-std::floor(py);
        Pixel result={};for(int j=0;j<2;j++)for(int i=0;i<2;i++)for(int c=0;c<4;c++)
            result[c]+=source[layer*size*size+((iy+j+size)%size)*size+(ix+i+size)%size][c]*(i?tx:1-tx)*(j?ty:1-ty);
        return result;
    };
    // Driver filtering may quantize bilinear fractions (the native AMD path
    // and llvmpipe need not equal a CPU float interpolator). Obtain source
    // endpoints through an independent minimal GPU sampler with the same
    // geometry/coordinates/gradients, without reproducing the mixture shader.
    const GLuint endpoint_program=program(controlled_vertex,R"GLSL(#version 330 core
in vec3 vPos;
uniform sampler2DArray tSource;
uniform vec2 tOffset;
uniform int tLayer;
out vec4 FragColor;
void main(){
    vec2 uv=vec2(vPos.x,-vPos.z)*.25;
    vec2 dx=dFdx(uv),dy=dFdy(uv);
    FragColor=textureGrad(tSource,vec3(uv+tOffset,float(tLayer)),dx,dy);
})GLSL");
    float endpoint_precision_delta=0;
    const auto sample=[&](int layer,float x,float z,bool alternate){
        glUseProgram(endpoint_program);sampler(endpoint_program,"tSource",10);sampler(endpoint_program,"tLayer",layer);
        vector(endpoint_program,"tPosition",x,24,z);glUniform2f(glGetUniformLocation(endpoint_program,"tWorldSpan"),0,0);
        glUniform2f(glGetUniformLocation(endpoint_program,"tOffset"),alternate?.371f:0,alternate?.619f:0);
        const Pixel value=render(endpoint_program);glUseProgram(land);
        const Pixel cpu=cpu_sample(layer,x*.25f+(alternate?.371f:0),-z*.25f+(alternate?.619f:0));
        for(int c=0;c<4;c++)endpoint_precision_delta=std::max(endpoint_precision_delta,std::abs(value[c]-cpu[c]));
        return value;
    };
    auto equal=[](const Pixel&a,const Pixel&b){for(int i=0;i<4;i++)assert(std::abs(a[i]-b[i])<3e-6f);};
    constexpr float cx=-1310.4f,cz=-10207.585f;
    vector(land,"tPosition",cx+1.37f,24,cz+2.43f);vector(land,"tNormal",0,1,0);vector(land,"tRelative",0,-3,0);
    glUniform2f(glGetUniformLocation(land,"tWorldSpan"),0,0);
    solid_texture(climate,6,{0,.5f,0,1},9);solid_texture(history,14,{0,0,0,0});
    glActiveTexture(GL_TEXTURE12);glBindTexture(GL_TEXTURE_2D_ARRAY,landscape);const Pixel paving={0,1,0,1};
    glTexSubImage3D(GL_TEXTURE_2D_ARRAY,0,0,0,3,1,1,1,GL_RGBA,GL_FLOAT,paving.data());
    sampler(land,"uHasStarting",0);sampler(land,"uHasVillage",1);sampler(land,"uVillagePBRReady",1);sampler(land,"uVillageGroundReady",1);sampler(land,"uVillageSkyEnabled",0);
    glUniform4f(glGetUniformLocation(land,"uVillageBounds"),cx-128,cz-128,1.f/256,1.f/256);
    glUniform4f(glGetUniformLocation(land,"uVillageGroundBounds"),cx-90,cz-70,1.f/180,1.f/180);
    vector(land,"uAmbient",0,0,0);vector(land,"uSkyLight",.3f,.3f,.3f);vector(land,"uGroundLight",0,0,0);vector(land,"uSunColor",0,0,0);
    scalar(land,"uCloud",0);scalar(land,"uHaze",0);scalar(land,"uWetness",0);scalar(land,"uTime",0);
    sampler(land,"uVillageStochasticPaving",1);
    // Infer the mixture from an AO-only marker under diffuse fill. This does
    // not duplicate the production noise field. The real map's AO drives the
    // selector and the same measured weight must then explain BC/N/roughness.
    Layers marker=source;std::fill_n(marker.begin(),size*size,Pixel{.4f,.4f,.4f,1});
    float x=cx+1.37f,z=cz+2.43f,weight=-1;std::array<Pixel,3>a,b;
    for(int attempt=0;attempt<20;attempt++){
        x=cx+1.37f+attempt*.61f;vector(land,"tPosition",x,24,z);
        put(source);
        for(int l=0;l<3;l++){a[l]=sample(l,x,z,false);b[l]=sample(l,x,z,true);}
        auto ma=a,mb=b;ma[0]=mb[0]={.4f,.4f,.4f,1};
        put(marker);sampler(land,"uVillageStochasticPaving",1);const Pixel actual=render(land);
        sampler(land,"uVillageStochasticPaving",0);put(constant(ma));const Pixel pa=render(land);put(constant(mb));const Pixel pb=render(land);
        if(std::abs(energy(pb)-energy(pa))<.002f)continue;
        weight=(energy(actual)-energy(pa))/(energy(pb)-energy(pa));
        if(weight>.08f&&weight<.92f)break;
    }
    assert(weight>.08f&&weight<.92f);
    const auto linear=[](float s){return s<=.04045f?s/12.92f:std::pow((s+.055f)/1.055f,2.4f);};
    const auto srgb=[](float v){return v<=.0031308f?v*12.92f:1.055f*std::pow(v,1/2.4f)-.055f;};
    std::array<Pixel,3> mixed;
    for(int l=0;l<3;l++)for(int c=0;c<4;c++)mixed[l][c]=a[l][c]*(1-weight)+b[l][c]*weight;
    for(int c=0;c<3;c++)mixed[0][c]=srgb(linear(a[0][c])*(1-weight)+linear(b[0][c])*weight);
    mixed[2][1]=std::sqrt(a[2][1]*a[2][1]*(1-weight)+b[2][1]*b[2][1]*weight);
    vector(land,"uSun",.3f,.9f,.3f);vector(land,"uSunColor",1,1,1);
    put(source);sampler(land,"uVillageStochasticPaving",1);const Pixel joined=render(land);
    put(constant(mixed));sampler(land,"uVillageStochasticPaving",0);const Pixel coherent=render(land);
    std::fprintf(stderr,"Stochastic coherence diagnostic: point(%.9g,%.9g),weight%.9g; joined(%.9g,%.9g,%.9g), reference(%.9g,%.9g,%.9g); sourceAO%.9g/%.9g,CPU/GPU endpoint delta%.9g\n",x,z,weight,joined[0],joined[1],joined[2],coherent[0],coherent[1],coherent[2],a[2][0],b[2][0],endpoint_precision_delta);
    for(int c=0;c<3;c++)assert(std::abs(joined[c]-coherent[c])<8e-5f);
    float channel_controls=0;
    for(int layer=0;layer<3;layer++){
        auto mismatched=mixed;mismatched[layer]=a[layer];put(constant(mismatched));const Pixel wrong=render(land);
        float difference=0;for(int c=0;c<3;c++)difference+=std::abs(wrong[c]-coherent[c]);
        assert(difference>1e-5f);channel_controls+=difference;
    }
    put(source);sampler(land,"uVillageStochasticPaving",0);const Pixel disabled=render(land);
    sampler(land,"uVillageStochasticPaving",1);const Pixel enabled=render(land);
    assert(std::abs(energy(disabled)-energy(enabled))>.0001f);
    scalar(land,"uTime",1e6f);vector(land,"uEye",45000,800,-25000);equal(enabled,render(land));scalar(land,"uTime",0);
    sampler(land,"uVillageGroundReady",0);equal(disabled,render(land));sampler(land,"uVillageGroundReady",1);
    glUniform4f(glGetUniformLocation(land,"uVillageGroundBounds"),1e6f,1e6f,1.f/180,1.f/180);equal(disabled,render(land));
    glUniform4f(glGetUniformLocation(land,"uVillageGroundBounds"),cx-90,cz-70,1.f/180,1.f/180);
    for(const char *flag:{"uHasVillage","uVillagePBRReady"}){
        sampler(land,flag,0);sampler(land,"uVillageStochasticPaving",0);const Pixel off=render(land);sampler(land,"uVillageStochasticPaving",1);equal(off,render(land));sampler(land,flag,1);
    }
    vector(land,"tPosition",x,43,z);sampler(land,"uVillageStochasticPaving",0);const Pixel high=render(land);sampler(land,"uVillageStochasticPaving",1);equal(high,render(land));
    // The selector must enter continuously through the actual two-metre
    // boundary, rather than making a branch-shaped color/material seam.
    vector(land,"uSunColor",0,0,0);float edge_effect=0;
    for(float offset:{-.01f,0.f,.01f}){
        vector(land,"tPosition",cx-90+offset,24,z);sampler(land,"uVillageStochasticPaving",0);const Pixel off=render(land);
        sampler(land,"uVillageStochasticPaving",1);const Pixel on=render(land);
        for(int c=0;c<3;c++)edge_effect=std::max(edge_effect,std::abs(on[c]-off[c]));
    }
    assert(edge_effect<2e-5f);
    // Vary real fragment world positions across the branch boundary and use
    // genuine mip chains. Repeated time/eye changes must not alter any pixel;
    // tiny footprint changes must not introduce mip-boundary noise.
    put(source,true);vector(land,"tPosition",cx-89,24,z);
    glUniform2f(glGetUniformLocation(land,"tWorldSpan"),32,24);
    const auto frame=[&](){render(land);std::vector<Pixel> result(side*side);glReadPixels(0,0,side,side,GL_RGBA,GL_FLOAT,result.data());return result;};
    const auto original=frame();scalar(land,"uTime",1e6f);vector(land,"uEye",-45000,-800,25000);assert(frame()==original);
    glUniform2f(glGetUniformLocation(land,"tWorldSpan"),32.001f,24.001f);const auto shifted=frame();float mip_delta=0;
    for(std::size_t i=0;i<original.size();i++)for(int c=0;c<3;c++)mip_delta=std::max(mip_delta,std::abs(original[i][c]-shifted[i][c]));
    assert(mip_delta<.002f);
    glUniform2f(glGetUniformLocation(land,"tWorldSpan"),0,0);sampler(land,"uVillageStochasticPaving",0);sampler(land,"uVillageGroundReady",0);sampler(land,"uHasVillage",0);scalar(land,"uTime",0);
    glActiveTexture(GL_TEXTURE10);glBindTexture(GL_TEXTURE_2D_ARRAY,pbr);glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glDeleteProgram(endpoint_program);
    std::printf("Lighting: nonconstant stochastic source BC/N/ARM share measured weight%.3f, channel controls%.5f; disabled/outside/elevated exact,2m edge effect%.7f,varying-world mip delta%.6f,time/eye frame exact PASS\n",weight,channel_controls,edge_effect,mip_delta);
}

void village_pbr_sky_tests(GLuint land,GLuint landscape,GLuint pbr,GLuint climate) {
    glUseProgram(land);sampler(land,"uVillagePBR",10);
    solid_texture(climate,6,{0,.5f,0,1},9);
    vector(land,"tPosition",-1310.4f,24,-10207.585f);vector(land,"tNormal",0,1,0);
    vector(land,"tRelative",0,-2,-5);vector(land,"uSun",.6f,.8f,0);
    vector(land,"uSunColor",0,0,0);vector(land,"uAmbient",0,0,0);
    vector(land,"uSkyLight",.3f,.3f,.3f);vector(land,"uGroundLight",0,0,0);
    scalar(land,"uCloud",0);scalar(land,"uHaze",0);
    const auto bounds=[&](bool inside){glUniform4f(glGetUniformLocation(land,"uVillageBounds"),
        inside?-1438.4f:1000.f,inside?-10335.585f:1000.f,1.f/256,1.f/256);};
    const auto mask=[&](float paving,float sky){
        glActiveTexture(GL_TEXTURE12);glBindTexture(GL_TEXTURE_2D_ARRAY,landscape);
        const Pixel value={0,paving,0,sky};
        glTexSubImage3D(GL_TEXTURE_2D_ARRAY,0,0,0,3,1,1,1,GL_RGBA,GL_FLOAT,value.data());
    };
    const auto albedo=[&](bool red){
        glActiveTexture(GL_TEXTURE10);glBindTexture(GL_TEXTURE_2D_ARRAY,pbr);
        const Pixel value=red?Pixel{.85f,.12f,.04f,1}:Pixel{.04f,.12f,.85f,1};
        glTexSubImage3D(GL_TEXTURE_2D_ARRAY,0,0,0,0,1,1,1,GL_RGBA,GL_FLOAT,value.data());
    };
    const auto same=[](const Pixel&a,const Pixel&b){for(int k=0;k<4;++k)assert(a[k]==b[k]);};
    bounds(true);mask(1,1);sampler(land,"uHasVillage",1);sampler(land,"uVillageSkyEnabled",0);
    sampler(land,"uVillagePBRReady",0);albedo(true);const Pixel disabled=render(land);
    albedo(false);same(disabled,render(land));
    sampler(land,"uVillagePBRReady",1);albedo(true);const Pixel red=render(land);
    albedo(false);const Pixel blue=render(land);
    assert(red[0]>red[2]*3&&blue[2]>blue[0]*3);
    const auto linear=[](float s){return s<=.04045f?s/12.92f:std::pow((s+.055f)/1.055f,2.4f);};
    const float decoded_ratio=linear(.85f)/linear(.12f);
    assert(std::abs(red[0]/red[1]-decoded_ratio)<decoded_ratio*.0001f);
    glActiveTexture(GL_TEXTURE10);glBindTexture(GL_TEXTURE_2D_ARRAY,pbr);
    const Pixel dark_arm={0,.8f,0,1};
    glTexSubImage3D(GL_TEXTURE_2D_ARRAY,0,0,0,2,1,1,1,GL_RGBA,GL_FLOAT,dark_arm.data());
    assert(std::abs(energy(render(land))/energy(blue)-.35f)<2e-5f);
    const Pixel open_arm={1,.8f,0,1};
    glTexSubImage3D(GL_TEXTURE_2D_ARRAY,0,0,0,2,1,1,1,GL_RGBA,GL_FLOAT,open_arm.data());
    mask(0,1);albedo(true);const Pixel no_paving=render(land);
    albedo(false);same(no_paving,render(land));
    sampler(land,"uVillagePBRReady",0);same(no_paving,render(land));
    mask(1,1);bounds(false);sampler(land,"uVillagePBRReady",0);const Pixel outside=render(land);
    sampler(land,"uVillagePBRReady",1);albedo(true);same(outside,render(land));
    albedo(false);same(outside,render(land));
    bounds(true);sampler(land,"uHasVillage",0);const Pixel no_village=render(land);
    sampler(land,"uVillagePBRReady",0);same(no_village,render(land));

    // Alpha is bounded sky access, independent of the optional PBR material.
    // With only explicit sky irradiance, the actual shader must attenuate it
    // by the measured mask, while preserving direct sunlight and ground control.
    sampler(land,"uHasVillage",1);sampler(land,"uVillageSkyEnabled",1);mask(1,1);
    const Pixel open=render(land);mask(1,0);const Pixel sheltered=render(land);
    const float ratio=energy(sheltered)/energy(open);assert(std::abs(ratio-.3f)<2e-5f);
    mask(1,.7f);assert(std::abs(energy(render(land))/energy(open)-.7f)<2e-5f);
    mask(1,0);sampler(land,"uVillageSkyEnabled",0);same(open,render(land));
    sampler(land,"uVillageSkyEnabled",1);
    vector(land,"uGroundLight",.2f,.2f,.2f);const Pixel bounce=render(land);
    vector(land,"uGroundLight",.4f,.4f,.4f);const Pixel double_bounce=render(land);
    for(int k=0;k<3;++k){
        assert(bounce[k]>sheltered[k]);
        assert(std::abs((double_bounce[k]-sheltered[k])-2*(bounce[k]-sheltered[k]))<1e-6f);
    }
    assert(energy(bounce)-energy(sheltered)<energy(open)*.2f);
    vector(land,"uGroundLight",0,0,0);vector(land,"uSkyLight",0,0,0);
    vector(land,"uSunColor",1,1,1);mask(1,1);const Pixel sun=render(land);
    mask(1,0);same(sun,render(land));
    vector(land,"uSunColor",0,0,0);vector(land,"uSkyLight",.3f,.3f,.3f);
    bounds(false);sampler(land,"uVillageSkyEnabled",0);const Pixel exterior=render(land);
    sampler(land,"uVillageSkyEnabled",1);same(exterior,render(land));
    bounds(true);sampler(land,"uHasVillage",0);sampler(land,"uVillageSkyEnabled",0);
    const Pixel world_only=render(land);sampler(land,"uVillageSkyEnabled",1);same(world_only,render(land));
    sampler(land,"uHasVillage",1);
    bounds(true);vector(land,"tPosition",-1310.4f,43,-10207.585f);
    sampler(land,"uVillageSkyEnabled",0);const Pixel above=render(land);
    sampler(land,"uVillageSkyEnabled",1);same(above,render(land));
    // Measured paving reflectance returns a bounded amount of actual source
    // irradiance. It is linear in material reflectance and emits no light.
    vector(land,"tPosition",-1310.4f,24,-10207.585f);mask(1,.5f);
    const Pixel unreturned=render(land);
    vector(land,"uVillageBounceAlbedo",.16384777f,.12907667f,.09379962f);
    const Pixel returned=render(land);
    vector(land,"uVillageBounceAlbedo",.16384777f*.5f,.12907667f*.5f,.09379962f*.5f);
    const Pixel half_return=render(land);
    for(int k=0;k<3;k++) {
        assert(returned[k]>unreturned[k]);
        assert(returned[k]-unreturned[k]<.01f);
        assert(std::fabs((half_return[k]-unreturned[k])*2-(returned[k]-unreturned[k]))<2e-6f);
    }
    bounds(false);vector(land,"uVillageBounceAlbedo",0,0,0);const Pixel outside_bounce=render(land);
    vector(land,"uVillageBounceAlbedo",.16384777f,.12907667f,.09379962f);same(outside_bounce,render(land));
    bounds(true);vector(land,"tPosition",-1310.4f,43,-10207.585f);
    vector(land,"uVillageBounceAlbedo",0,0,0);const Pixel above_bounce=render(land);
    vector(land,"uVillageBounceAlbedo",.16384777f,.12907667f,.09379962f);same(above_bounce,render(land));
    vector(land,"tPosition",-1310.4f,24,-10207.585f);vector(land,"uSkyLight",0,0,0);
    assert(energy(render(land))==0);vector(land,"uVillageBounceAlbedo",0,0,0);
    sampler(land,"uHasVillage",0);sampler(land,"uVillageSkyEnabled",0);sampler(land,"uVillagePBRReady",0);
    std::printf("Lighting: source village PBR sRGB/ARM and bounds/paving/ready confinement; sky ratio%.3f, direct exact, bounded linear ground return, roof/exterior exact PASS\n",ratio);
}

void sky_tests(GLuint sky) {
    glUseProgram(sky);
    vector(sky, "uRight", 0, 0, 0); vector(sky, "uUp", 0, 0, 0); vector(sky, "uForward", 0, 1, 0);
    vector(sky, "uSun", .6f, .8f, 0); vector(sky, "uSunColor", 1.2f, 1.1f, .9f);
    vector(sky, "uSkyLight", .25f, .30f, .38f); vector(sky, "uGroundLight", .07f, .065f, .05f);
    vector(sky, "uSkyZenith", .04f, .12f, .28f); vector(sky, "uFog", .3f, .4f, .5f);
    scalar(sky, "uAspect", 1); scalar(sky, "uTan", .6f);
    scalar(sky, "uCloud", 0); vector(sky, "uEye", 0, 100, 0);
    Pixel clear = render(sky);
    float largest_difference = 0, chosen_x = 0;
    Pixel chosen = {};
    scalar(sky, "uCloud", 1);
    for (int i = 0; i < 48; ++i) {
        vector(sky, "uEye", (i - 24) * 800.f, 100, 0);
        Pixel cloudy = render(sky);
        float difference = std::abs(energy(cloudy) - energy(clear));
        if (difference > largest_difference) {
            largest_difference = difference; chosen = cloudy; chosen_x = (i - 24) * 800.f;
        }
    }
    assert(largest_difference > .02f);
    // Match the same moving cloud coordinate after 100 seconds by moving the
    // observer against wind. A screen-anchored cloud implementation fails this.
    vector(sky, "uEye", chosen_x - .14f / .00042f, 100, .055f / .00042f);
    scalar(sky, "uTime", 100); Pixel anchored = render(sky);
    for (int i = 0; i < 3; ++i) assert(std::abs(chosen[i] - anchored[i]) < 5e-4f);
    scalar(sky, "uCloud", 0); Pixel cloud_off = render(sky);
    for (int i = 0; i < 3; ++i) assert(std::abs(clear[i] - cloud_off[i]) < 1e-5f);
    vector(sky, "uEye", chosen_x, 4500, 0);
    scalar(sky, "uCloud", 0); Pixel clear_above = render(sky);
    scalar(sky, "uCloud", 1); Pixel above_clouds = render(sky);
    for (int i = 0; i < 3; ++i) assert(std::abs(clear_above[i] - above_clouds[i]) < 1e-5f);
    // The bounded cloud volume must fade into the aerosol horizon, rather than
    // presenting high-contrast perspective-compressed stripes there.
    vector(sky, "uEye", chosen_x, 100, 0); vector(sky, "uForward", 1, .04f, 0);
    // Overcast intentionally changes the broad horizon. Hold that sheet
    // fixed and toggle only the volume to detect compressed cloud stripes.
    scalar(sky, "uCloud", 1); sampler(sky,"uVolumetricClouds",0);
    Pixel clear_horizon = render(sky);
    sampler(sky,"uVolumetricClouds",1);
    for (int position = 0; position < 48; ++position) {
        vector(sky, "uEye", (position - 24) * 800.f, 100, 0);
        Pixel cloud_horizon = render(sky);
        for (int i = 0; i < 3; ++i) assert(std::abs(clear_horizon[i] - cloud_horizon[i]) < 1e-5f);
    }
    // Actual screen rays at the horizon, night-like input and the HDR sun disc
    // remain finite. These are validity checks, not an art-quality oracle.
    vector(sky, "uRight", 1, 0, 0); vector(sky, "uUp", 0, 1, 0); vector(sky, "uForward", 0, 0, 1);
    vector(sky, "uEye", 50000, 0, 25000); scalar(sky, "uTime", 1e6f); (void)render(sky);
    vector(sky, "uSunColor", 0, 0, 0); vector(sky, "uSkyLight", 0, 0, 0);
    vector(sky, "uAmbient", 0, 0, 0); (void)render(sky);
    std::printf("Lighting: world-anchored clouds, zero-cloud, above-layer and horizon-fade controls PASS\n");
}
}

int main(int argc, char **argv) {
    if (argc > 1) assert(chdir(argv[1]) == 0);
    if (!std::getenv("VALWOOD_TEST_NATIVE_GL")) SDL_setenv("SDL_VIDEODRIVER", "offscreen", 1);
    assert(SDL_Init(SDL_INIT_VIDEO) == 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_Window *window = SDL_CreateWindow("World lighting test", 0, 0, side, side, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    assert(window); SDL_GLContext context = SDL_GL_CreateContext(window); assert(context);
    std::printf("Lighting test renderer: %s\n", glGetString(GL_RENDERER));
    GLuint vao; glGenVertexArrays(1, &vao); glBindVertexArray(vao);
    GLuint target, fbo; glGenTextures(1, &target); glBindTexture(GL_TEXTURE_2D, target);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, side, side, 0, GL_RGBA, GL_FLOAT, nullptr);
    glGenFramebuffers(1, &fbo); glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, target, 0);
    assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
    glViewport(0, 0, side, side); glDisable(GL_DEPTH_TEST); glDisable(GL_BLEND);
    std::vector<GLuint> textures;
    for (int unit = 0; unit < 15; ++unit) {
        if (unit == 10 || unit == 12 || unit == 13) continue;
        Pixel value = {.4f, .4f, .4f, 1};
        if (unit == 4 || unit == 5) value = {.5f, .5f, 1, 1};
        if (unit == 6) value = {0, .5f, 0, 1};
        if (unit == 7) value = {1, 1, 1, 1};
        if (unit == 8 || unit == 9) value = {1, .8f, 0, 1};
        if (unit == 14) value = {0,0,0,0};
        textures.push_back(texture(unit, value));
    }
    GLuint macro_height = textures[10]; // Units 0..9 followed by unit 11.
    GLuint litter_pbr;glGenTextures(1,&litter_pbr);glActiveTexture(GL_TEXTURE13);glBindTexture(GL_TEXTURE_2D_ARRAY,litter_pbr);
    const float litter_layers[]={.5f,.5f,1,1,1,.8f,0,1};
    glTexImage3D(GL_TEXTURE_2D_ARRAY,0,GL_RGBA32F,1,1,2,0,GL_RGBA,GL_FLOAT,litter_layers);
    glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    height_texture(macro_height, 0);
    GLuint landscape; glGenTextures(1, &landscape); glActiveTexture(GL_TEXTURE12);
    glBindTexture(GL_TEXTURE_2D_ARRAY, landscape);
    const float empty[16] = {};
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA32F, 1, 1, 4, 0, GL_RGBA, GL_FLOAT, empty);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    GLuint village_pbr;glGenTextures(1,&village_pbr);glActiveTexture(GL_TEXTURE10);
    glBindTexture(GL_TEXTURE_2D_ARRAY,village_pbr);
    const float pbr_layers[12]={.4f,.4f,.4f,1,.5f,.5f,1,1,1,.8f,0,1};
    glTexImage3D(GL_TEXTURE_2D_ARRAY,0,GL_RGBA32F,1,1,3,0,GL_RGBA,GL_FLOAT,pbr_layers);
    glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    std::string terrain_source = read_file("shaders/world_terrain.fs.glsl");
    GLuint production = program(read_file("shaders/world_terrain.vs.glsl"), terrain_source);
    GLuint land = program(controlled_vertex, terrain_source);
    GLuint sky = program(read_file("shaders/world_sky.vs.glsl"), read_file("shaders/world_sky.fs.glsl"));
    terrain_tests(land, macro_height, textures[2], textures[6]);
    rainforest_tests(land, textures[6],textures[3]);freshwater_bank_tests(land,textures[2],textures[6]);
    local_irradiance_tests(land);village_layer_tests(land, landscape, textures[6]);
    ground_surface_tests(land,landscape,textures[6],textures[0],textures[3]);
    village_pbr_sky_tests(land,landscape,village_pbr,textures[6]);
    village_ground_history_tests(land,landscape,village_pbr,litter_pbr,textures[11],textures[6],textures[3]);
    village_stochastic_paving_tests(land,landscape,village_pbr,textures[11],textures[6]);sky_tests(sky);
    glDeleteProgram(production); glDeleteProgram(land); glDeleteProgram(sky);
    glDeleteTextures(static_cast<GLsizei>(textures.size()), textures.data()); glDeleteTextures(1, &landscape);
    glDeleteTextures(1,&village_pbr);
    glDeleteTextures(1,&litter_pbr);
    glDeleteFramebuffers(1, &fbo); glDeleteTextures(1, &target); glDeleteVertexArrays(1, &vao);
    assert(glGetError() == GL_NO_ERROR);
    SDL_GL_DeleteContext(context); SDL_DestroyWindow(window); SDL_Quit();
    std::puts("world_lighting_gl_test: PASS");
}
