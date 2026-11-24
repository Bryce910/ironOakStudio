#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include "engine/world_temporal.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <new>
#include <sstream>
#include <string>

namespace {
constexpr int max_dimension = 8192;
constexpr std::uint64_t max_pixels = 4096ull * 4096ull;

bool inverse(const float matrix[16], double out[16]) {
    if (!matrix) return false;
    double a[4][8] = {};
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            float value = matrix[col * 4 + row];
            if (!std::isfinite(value)) return false;
            a[row][col] = value;
        }
        a[row][row + 4] = 1.;
    }
    for (int col = 0; col < 4; ++col) {
        int pivot = col;
        for (int row = col + 1; row < 4; ++row)
            if (std::abs(a[row][col]) > std::abs(a[pivot][col])) pivot = row;
        if (std::abs(a[pivot][col]) < 1.e-12) return false;
        for (int k = 0; k < 8; ++k) std::swap(a[pivot][k], a[col][k]);
        double divisor = a[col][col];
        for (double &value : a[col]) value /= divisor;
        for (int row = 0; row < 4; ++row) {
            if (row == col) continue;
            double factor = a[row][col];
            for (int k = 0; k < 8; ++k) a[row][k] -= factor * a[col][k];
        }
    }
    for (int col = 0; col < 4; ++col)
        for (int row = 0; row < 4; ++row) out[col * 4 + row] = a[row][col + 4];
    return true;
}

bool reprojection(const double inv[16], const float previous[16], const double delta[3], float out[16]) {
    double result[16] = {};
    for (int col = 0; col < 4; ++col) {
        double translated[4];
        for (int row = 0; row < 3; ++row)
            translated[row] = inv[col * 4 + row] + delta[row] * inv[col * 4 + 3];
        translated[3] = inv[col * 4 + 3];
        for (int row = 0; row < 4; ++row)
            for (int k = 0; k < 4; ++k)
                result[col * 4 + row] += double(previous[k * 4 + row]) * translated[k];
    }
    for (int k = 0; k < 16; ++k)
        if (!std::isfinite(result[k]) || std::abs(result[k]) > 1.e30) return false;
    for (int k = 0; k < 16; ++k) out[k] = float(result[k]);
    return true;
}

GLuint compile_shader(GLenum type, const char *path) {
    if (!path) return 0;
    std::ifstream file(path, std::ios::binary);
    if (!file) { std::fprintf(stderr, "[Temporal] Missing shader %s\n", path); return 0; }
    std::ostringstream stream;
    stream << file.rdbuf();
    std::string source = stream.str();
    if (source.empty() || source.size() > 1024 * 1024) return 0;
    GLuint shader = glCreateShader(type);
    const char *text = source.c_str();
    glShaderSource(shader, 1, &text, nullptr);
    glCompileShader(shader);
    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048] = {};
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        std::fprintf(stderr, "[Temporal] %s: %s\n", path, log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

float halton(unsigned index, unsigned base) {
    float value = 0, fraction = 1;
    while (index) {
        fraction /= float(base);
        value += fraction * float(index % base);
        index /= base;
    }
    return value;
}
}

bool world_temporal_reprojection(const float current[16], const float previous[16],
                                 const double current_origin[3], const double previous_origin[3],
                                 float output[16]) {
    if (!current || !previous || !current_origin || !previous_origin || !output) return false;
    double inv[16], previous_inverse[16], delta[3];
    if (!inverse(current, inv) || !inverse(previous, previous_inverse)) return false;
    for (int k = 0; k < 3; ++k) {
        delta[k] = current_origin[k] - previous_origin[k];
        if (!std::isfinite(delta[k])) return false;
    }
    return reprojection(inv, previous, delta, output);
}

void world_temporal_jitter(std::uint64_t sample, float jitter[2]) {
    if (!jitter) return;
    unsigned index = unsigned(sample % 8) + 1;
    jitter[0] = halton(index, 2) - .5f;
    jitter[1] = halton(index, 3) - .5f;
}

struct WorldTemporal {
    GLuint program = 0, vao = 0, fbo[2] = {}, color[2] = {}, depth[2] = {};
    // Input effects may intentionally retain point-filtered render targets.
    // Reconstruction must not depend on those texture objects or on sampler
    // state left by a previous pass. These two loading-owned GL3.3 objects
    // enforce linear color and point depth without mutating borrowed inputs.
    GLuint samplers[2] = {}; // linear color, point depth
    GLint current_color = -1, current_depth = -1, history_color = -1, history_depth = -1;
    GLint reprojection = -1, sky_reprojection = -1, near_far = -1, previous_near_far = -1;
    GLint size = -1, has_history = -1, weight = -1, reactive_alpha = -1, object_motion = -1, has_object_motion = -1;
    GLint current_jitter = -1, previous_jitter = -1;
    int width = 0, height = 0, front = 0;
    bool valid = false, has_output = false;
    float previous_vp[16] = {}, previous_near = .4f, previous_far = 15000.f;
    float previous_jitter_uv[2] = {};
    double previous_camera[3] = {}, previous_forward[3] = {};

    void release_targets() {
        glDeleteFramebuffers(2, fbo);
        glDeleteTextures(2, color);
        glDeleteTextures(2, depth);
        for (int k = 0; k < 2; ++k) fbo[k] = color[k] = depth[k] = 0;
        width = height = 0;
        valid = has_output = false;
    }

    bool resize(int w, int h) {
        if (w == width && h == height) return true;
        release_targets();
        glGenFramebuffers(2, fbo);
        glGenTextures(2, color);
        glGenTextures(2, depth);
        const GLenum attachments[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
        const GLfloat zero[4] = {};
        glDisable(GL_SCISSOR_TEST);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        for (int k = 0; k < 2; ++k) {
            glBindFramebuffer(GL_FRAMEBUFFER, fbo[k]);
            for (int attachment = 0; attachment < 2; ++attachment) {
                glBindTexture(GL_TEXTURE_2D, attachment ? depth[k] : color[k]);
                glTexImage2D(GL_TEXTURE_2D, 0, attachment ? GL_R32F : GL_RGBA16F, w, h, 0,
                             attachment ? GL_RED : GL_RGBA, GL_FLOAT, nullptr);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, attachment ? GL_NEAREST : GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, attachment ? GL_NEAREST : GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glFramebufferTexture2D(GL_FRAMEBUFFER, attachments[attachment], GL_TEXTURE_2D,
                                       attachment ? depth[k] : color[k], 0);
            }
            glDrawBuffers(2, attachments);
            if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
                std::fprintf(stderr, "[Temporal] Incomplete history framebuffer %dx%d\n", w, h);
                release_targets();
                return false;
            }
            glClearBufferfv(GL_COLOR, 0, zero);
            glClearBufferfv(GL_COLOR, 1, zero);
        }
        width = w; height = h; front = 0;
        return true;
    }
};

WorldTemporal *world_temporal_create(const char *vertex_path, const char *fragment_path) {
    if (!SDL_GL_GetCurrentContext()) return nullptr;
    GLuint vertex = compile_shader(GL_VERTEX_SHADER, vertex_path);
    GLuint fragment = compile_shader(GL_FRAGMENT_SHADER, fragment_path);
    if (!vertex || !fragment) { glDeleteShader(vertex); glDeleteShader(fragment); return nullptr; }
    WorldTemporal *temporal = new (std::nothrow) WorldTemporal;
    if (!temporal) { glDeleteShader(vertex); glDeleteShader(fragment); return nullptr; }
    temporal->program = glCreateProgram();
    glAttachShader(temporal->program, vertex);
    glAttachShader(temporal->program, fragment);
    glLinkProgram(temporal->program);
    glDeleteShader(vertex); glDeleteShader(fragment);
    GLint linked = GL_FALSE;
    glGetProgramiv(temporal->program, GL_LINK_STATUS, &linked);
    if (!linked) {
        char log[2048] = {};
        glGetProgramInfoLog(temporal->program, sizeof(log), nullptr, log);
        std::fprintf(stderr, "[Temporal] Link: %s\n", log);
        world_temporal_free(temporal);
        return nullptr;
    }
#define TEMPORAL_UNIFORM(member, name) temporal->member = glGetUniformLocation(temporal->program, name)
    TEMPORAL_UNIFORM(current_color, "uCurrentColor");
    TEMPORAL_UNIFORM(current_depth, "uCurrentDepth");
    TEMPORAL_UNIFORM(history_color, "uHistoryColor");
    TEMPORAL_UNIFORM(history_depth, "uHistoryDepth");
    TEMPORAL_UNIFORM(reprojection, "uReprojection");
    TEMPORAL_UNIFORM(sky_reprojection, "uSkyReprojection");
    TEMPORAL_UNIFORM(near_far, "uNearFar");
    TEMPORAL_UNIFORM(previous_near_far, "uPreviousNearFar");
    TEMPORAL_UNIFORM(size, "uResolution");
    TEMPORAL_UNIFORM(has_history, "uHasHistory");
    TEMPORAL_UNIFORM(weight, "uHistoryWeight");
    TEMPORAL_UNIFORM(reactive_alpha, "uReactiveAlpha");
    TEMPORAL_UNIFORM(object_motion, "uObjectMotion");
    TEMPORAL_UNIFORM(has_object_motion, "uHasObjectMotion");
    TEMPORAL_UNIFORM(current_jitter, "uCurrentJitterUV");
    TEMPORAL_UNIFORM(previous_jitter, "uPreviousJitterUV");
#undef TEMPORAL_UNIFORM
    glGenVertexArrays(1, &temporal->vao);
    glGenSamplers(2, temporal->samplers);
    if(!temporal->vao||!temporal->samplers[0]||!temporal->samplers[1]){
        world_temporal_free(temporal);return nullptr;
    }
    for(unsigned i=0;i<2;i++){
        const GLint filter=i?GL_NEAREST:GL_LINEAR;
        glSamplerParameteri(temporal->samplers[i],GL_TEXTURE_MIN_FILTER,filter);
        glSamplerParameteri(temporal->samplers[i],GL_TEXTURE_MAG_FILTER,filter);
        glSamplerParameteri(temporal->samplers[i],GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
        glSamplerParameteri(temporal->samplers[i],GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        glSamplerParameteri(temporal->samplers[i],GL_TEXTURE_COMPARE_MODE,GL_NONE);
    }
    return temporal;
}

unsigned int world_temporal_resolve(WorldTemporal *temporal, const WorldTemporalFrame &frame) {
    if (!temporal) return frame.color;
    auto fail = [&]() {
        temporal->valid = temporal->has_output = false;
        for(GLuint unit=0;unit<5;unit++)glBindSampler(unit,0);
        return frame.color;
    };
    if (!frame.color || !frame.depth || frame.width <= 0 || frame.height <= 0 ||
        frame.width > max_dimension || frame.height > max_dimension ||
        std::uint64_t(frame.width) * std::uint64_t(frame.height) > max_pixels ||
        !std::isfinite(frame.near_plane) || !std::isfinite(frame.far_plane) ||
        !std::isfinite(frame.jitter_pixels[0]) || !std::isfinite(frame.jitter_pixels[1]) ||
        std::abs(frame.jitter_pixels[0]) > .5f || std::abs(frame.jitter_pixels[1]) > .5f ||
        frame.near_plane <= 0 || frame.far_plane <= frame.near_plane) return fail();
    double inv[16], delta[3], forward[3];
    if (!inverse(frame.view_projection, inv)) return fail();
    double length = 0, distance = 0;
    for (int k = 0; k < 3; ++k) {
        if (!std::isfinite(frame.camera[k])) return fail();
        delta[k] = frame.camera[k] - temporal->previous_camera[k];
        distance += delta[k] * delta[k];
        forward[k] = inv[8 + k] + inv[12 + k];
        length += forward[k] * forward[k];
    }
    if (!std::isfinite(length) || length < 1.e-12) return fail();
    double direction_dot = 0;
    for (int k = 0; k < 3; ++k) {
        forward[k] /= std::sqrt(length);
        direction_dot += forward[k] * temporal->previous_forward[k];
    }
    if (frame.reset || distance > 80. * 80. || direction_dot < .70710678 ||
        frame.near_plane != temporal->previous_near || frame.far_plane != temporal->previous_far)
        temporal->valid = false;
    if (!temporal->resize(frame.width, frame.height)) return fail();
    float geometry_matrix[16] = {}, sky_matrix[16] = {};
    const double no_translation[3] = {};
    if (temporal->valid && (!reprojection(inv, temporal->previous_vp, delta, geometry_matrix) ||
        !reprojection(inv, temporal->previous_vp, no_translation, sky_matrix))) return fail();
    int output = 1 - temporal->front;
    glBindFramebuffer(GL_FRAMEBUFFER, temporal->fbo[output]);
    glViewport(0, 0, frame.width, frame.height);
    glDisable(GL_DEPTH_TEST); glDisable(GL_BLEND); glDisable(GL_CULL_FACE); glDisable(GL_SCISSOR_TEST);
    glDepthMask(GL_FALSE); glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glUseProgram(temporal->program);
    const GLuint textures[] = {frame.color, frame.depth, temporal->color[temporal->front], temporal->depth[temporal->front]};
    for (int k = 0; k < 4; ++k) {
        glActiveTexture(GL_TEXTURE0 + k); glBindTexture(GL_TEXTURE_2D, textures[k]);
        glBindSampler(GLuint(k),temporal->samplers[k%2]);
    }
    glUniform1i(temporal->current_color, 0); glUniform1i(temporal->current_depth, 1);
    glUniform1i(temporal->history_color, 2); glUniform1i(temporal->history_depth, 3);
    glUniformMatrix4fv(temporal->reprojection, 1, GL_FALSE, geometry_matrix);
    glUniformMatrix4fv(temporal->sky_reprojection, 1, GL_FALSE, sky_matrix);
    glUniform2f(temporal->near_far, frame.near_plane, frame.far_plane);
    glUniform2f(temporal->previous_near_far, temporal->previous_near, temporal->previous_far);
    glUniform2f(temporal->size, float(frame.width), float(frame.height));
    glUniform2f(temporal->current_jitter, frame.jitter_pixels[0] / frame.width, frame.jitter_pixels[1] / frame.height);
    glUniform2f(temporal->previous_jitter, temporal->previous_jitter_uv[0], temporal->previous_jitter_uv[1]);
    glUniform1i(temporal->has_history, temporal->valid);
    glUniform1i(temporal->reactive_alpha, frame.reactive_alpha);
    glActiveTexture(GL_TEXTURE4); glBindTexture(GL_TEXTURE_2D, frame.object_motion);
    glBindSampler(4, temporal->samplers[1]);
    glUniform1i(temporal->object_motion, 4);
    glUniform1i(temporal->has_object_motion, frame.object_motion != 0);
    float weight = std::isfinite(frame.history_weight) ? std::clamp(frame.history_weight, 0.f, .95f) : .90f;
    glUniform1f(temporal->weight, weight);
    glBindVertexArray(temporal->vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    for(GLuint unit=0;unit<5;unit++)glBindSampler(unit,0);
    glActiveTexture(GL_TEXTURE4); glBindTexture(GL_TEXTURE_2D, 0); glActiveTexture(GL_TEXTURE0);
    std::copy(frame.view_projection, frame.view_projection + 16, temporal->previous_vp);
    for (int k = 0; k < 3; ++k) {
        temporal->previous_camera[k] = frame.camera[k];
        temporal->previous_forward[k] = forward[k];
    }
    temporal->previous_near = frame.near_plane; temporal->previous_far = frame.far_plane;
    temporal->previous_jitter_uv[0] = frame.jitter_pixels[0] / frame.width;
    temporal->previous_jitter_uv[1] = frame.jitter_pixels[1] / frame.height;
    temporal->front = output; temporal->valid = temporal->has_output = true;
    return temporal->color[output];
}

unsigned int world_temporal_output(const WorldTemporal *temporal, unsigned int fallback) {
    return temporal && temporal->has_output ? temporal->color[temporal->front] : fallback;
}

unsigned int world_temporal_depth(const WorldTemporal *temporal, unsigned int fallback) {
    return temporal && temporal->has_output ? temporal->depth[temporal->front] : fallback;
}

void world_temporal_reset(WorldTemporal *temporal) {
    if (temporal) temporal->valid = false;
}

void world_temporal_free(WorldTemporal *temporal) {
    if (!temporal) return;
    temporal->release_targets();
    glDeleteSamplers(2,temporal->samplers);
    glDeleteVertexArrays(1, &temporal->vao);
    glDeleteProgram(temporal->program);
    delete temporal;
}
