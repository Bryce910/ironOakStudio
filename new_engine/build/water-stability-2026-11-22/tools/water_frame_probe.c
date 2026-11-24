#define _GNU_SOURCE
#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_opengl_glext.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* Diagnostic only: synchronous RGB readback at the presentation boundary.
 * No game source, render target contents, GL error flags or viewport are changed.
 * The renderer is single-context/single-threaded. TLS prevents re-entrant hooks.
 */
typedef void (*SwapFn)(SDL_Window *);
typedef void (APIENTRY *MatrixFn)(GLint, GLsizei, GLboolean, const GLfloat *);
static SwapFn real_swap;
static MatrixFn real_matrix;
static unsigned long long swap_number;
static unsigned captured;
static int started, finished, configured;
static const char *prefix, *gate;
static FILE *matrix_file, *frame_file;
static __thread int inside;
static unsigned char *pixels;
static size_t pixel_capacity;

static unsigned long long monotonic_ns(void) {
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now)) return 0;
    return (unsigned long long)now.tv_sec * 1000000000ULL + (unsigned long long)now.tv_nsec;
}

static FILE *new_file(const char *suffix) {
    char path[4096];
    if (snprintf(path, sizeof path, "%s%s", prefix, suffix) >= (int)sizeof path) return NULL;
    int fd = open(path, O_CREAT | O_EXCL | O_WRONLY | O_CLOEXEC, 0644);
    if (fd < 0) {
        fprintf(stderr, "[water probe] Cannot create %s: %s\n", path, strerror(errno));
        return NULL;
    }
    FILE *file = fdopen(fd, "wb");
    if (!file) close(fd);
    return file;
}

static int capture_active(void) {
    if (!configured) {
        configured = 1;
        prefix = getenv("VALWOOD_WATER_CAPTURE_PREFIX");
        gate = getenv("VALWOOD_WATER_CAPTURE_GATE");
    }
    if (finished || !prefix || !*prefix || !gate || !*gate) return 0;
    if (!started) {
        if (access(gate, F_OK)) return 0;
        started = 1;
        matrix_file = new_file("-matrices.csv");
        frame_file = new_file("-frames.csv");
        if (!matrix_file || !frame_file) {
            finished = 1;
            if (matrix_file) fclose(matrix_file);
            if (frame_file) fclose(frame_file);
            matrix_file = frame_file = NULL;
            return 0;
        }
        fprintf(matrix_file, "swap,capture_index,program,location,count,transpose,monotonic_ns");
        for (unsigned i = 0; i < 16; ++i) fprintf(matrix_file, ",m%u", i);
        fputc('\n', matrix_file);
        fprintf(frame_file, "swap,capture_index,width,height,read_buffer,monotonic_ns,bytes,status\n");
        fflush(matrix_file);
        fflush(frame_file);
        fprintf(stderr, "[water probe] Gate opened; capturing 32 consecutive presentation swaps to %s-000.ppm ...\n", prefix);
    }
    return captured < 32;
}

void APIENTRY glUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) {
    if (!real_matrix) real_matrix = (MatrixFn)dlsym(RTLD_NEXT, "glUniformMatrix4fv");
    if (!real_matrix) {
        fprintf(stderr, "[water probe] Cannot resolve real glUniformMatrix4fv\n");
        return;
    }
    /* Record the upload exactly as supplied, but always forward it unchanged. */
    if (!inside && location >= 0 && count > 0 && value) {
        inside = 1;
        if (capture_active() && SDL_GL_GetCurrentContext()) {
            GLint program = 0;
            glGetIntegerv(GL_CURRENT_PROGRAM, &program);
            if (program > 0 && location == glGetUniformLocation((GLuint)program, "uReflectionVP")) {
                fprintf(matrix_file, "%llu,%u,%d,%d,%d,%u,%llu", swap_number + 1, captured,
                        program, location, count, (unsigned)transpose, monotonic_ns());
                for (unsigned i = 0; i < 16; ++i) fprintf(matrix_file, ",%.9g", (double)value[i]);
                fputc('\n', matrix_file);
                fflush(matrix_file);
            }
        }
        inside = 0;
    }
    real_matrix(location, count, transpose, value);
}

struct PackState {
    GLint alignment, row_length, skip_rows, skip_pixels, image_height, skip_images;
    GLint swap_bytes, lsb_first, pbo, read_fbo, read_buffer, default_read_buffer;
};

static void read_back_rgb(int width, int height, unsigned char *destination, GLenum *read_source) {
    struct PackState s;
    glGetIntegerv(GL_PACK_ALIGNMENT, &s.alignment);
    glGetIntegerv(GL_PACK_ROW_LENGTH, &s.row_length);
    glGetIntegerv(GL_PACK_SKIP_ROWS, &s.skip_rows);
    glGetIntegerv(GL_PACK_SKIP_PIXELS, &s.skip_pixels);
    glGetIntegerv(GL_PACK_IMAGE_HEIGHT, &s.image_height);
    glGetIntegerv(GL_PACK_SKIP_IMAGES, &s.skip_images);
    glGetIntegerv(GL_PACK_SWAP_BYTES, &s.swap_bytes);
    glGetIntegerv(GL_PACK_LSB_FIRST, &s.lsb_first);
    glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &s.pbo);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &s.read_fbo);
    glGetIntegerv(GL_READ_BUFFER, &s.read_buffer);

    /* Binding only READ leaves the renderer's DRAW binding completely intact. */
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glGetIntegerv(GL_READ_BUFFER, &s.default_read_buffer);
    GLboolean double_buffer = GL_FALSE;
    glGetBooleanv(GL_DOUBLEBUFFER, &double_buffer);
    *read_source = double_buffer ? GL_BACK : GL_FRONT;
    glReadBuffer(*read_source);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glPixelStorei(GL_PACK_ROW_LENGTH, 0);
    glPixelStorei(GL_PACK_SKIP_ROWS, 0);
    glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
    glPixelStorei(GL_PACK_IMAGE_HEIGHT, 0);
    glPixelStorei(GL_PACK_SKIP_IMAGES, 0);
    glPixelStorei(GL_PACK_SWAP_BYTES, GL_FALSE);
    glPixelStorei(GL_PACK_LSB_FIRST, GL_FALSE);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, destination);

    glPixelStorei(GL_PACK_ALIGNMENT, s.alignment);
    glPixelStorei(GL_PACK_ROW_LENGTH, s.row_length);
    glPixelStorei(GL_PACK_SKIP_ROWS, s.skip_rows);
    glPixelStorei(GL_PACK_SKIP_PIXELS, s.skip_pixels);
    glPixelStorei(GL_PACK_IMAGE_HEIGHT, s.image_height);
    glPixelStorei(GL_PACK_SKIP_IMAGES, s.skip_images);
    glPixelStorei(GL_PACK_SWAP_BYTES, s.swap_bytes);
    glPixelStorei(GL_PACK_LSB_FIRST, s.lsb_first);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, (GLuint)s.pbo);
    glReadBuffer((GLenum)s.default_read_buffer);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)s.read_fbo);
    /* READ_BUFFER belongs to its FBO. Explicit restoration also handles FBO 0. */
    glReadBuffer((GLenum)s.read_buffer);
}

static void capture(SDL_Window *window) {
    int width = 0, height = 0;
    SDL_GL_GetDrawableSize(window, &width, &height);
    const unsigned long long ns = monotonic_ns();
    if (width <= 0 || height <= 0 || width > 32768 || height > 32768 ||
        (size_t)width > SIZE_MAX / (size_t)height / 3) {
        fprintf(frame_file, "%llu,%u,%d,%d,0,%llu,0,invalid_dimensions\n", swap_number + 1, captured, width, height, ns);
        return;
    }
    size_t bytes = (size_t)width * (size_t)height * 3;
    if (bytes > pixel_capacity) {
        unsigned char *next = realloc(pixels, bytes);
        if (!next) {
            fprintf(frame_file, "%llu,%u,%d,%d,0,%llu,%zu,allocation_failed\n", swap_number + 1, captured, width, height, ns, bytes);
            return;
        }
        pixels = next;
        pixel_capacity = bytes;
    }
    GLenum read_source;
    read_back_rgb(width, height, pixels, &read_source);
    char suffix[64];
    snprintf(suffix, sizeof suffix, "-%03u.ppm", captured);
    FILE *file = new_file(suffix);
    int success = file != NULL;
    if (file) {
        if (fprintf(file, "P6\n%d %d\n255\n", width, height) < 0) success = 0;
        const size_t stride = (size_t)width * 3;
        /* OpenGL origin is bottom-left; PPM is written top-left. */
        for (int row = height - 1; row >= 0; --row)
            if (fwrite(pixels + (size_t)row * stride, 1, stride, file) != stride) success = 0;
        if (fclose(file)) success = 0;
    }
    fprintf(frame_file, "%llu,%u,%d,%d,0x%x,%llu,%zu,%s\n", swap_number + 1, captured,
            width, height, read_source, ns, bytes, success ? "ok" : "write_failed");
}

void SDL_GL_SwapWindow(SDL_Window *window) {
    if (!real_swap) real_swap = (SwapFn)dlsym(RTLD_NEXT, "SDL_GL_SwapWindow");
    if (!real_swap) {
        fprintf(stderr, "[water probe] Cannot resolve real SDL_GL_SwapWindow\n");
        return;
    }
    if (!inside) {
        inside = 1;
        if (capture_active() && SDL_GL_GetCurrentContext()) {
            capture(window);
            ++captured;
            fflush(frame_file);
            if (captured == 32) {
                finished = 1;
                fclose(matrix_file);
                fclose(frame_file);
                matrix_file = frame_file = NULL;
                fprintf(stderr, "[water probe] Finished 32 consecutive swaps; readback disabled.\n");
            }
        }
        inside = 0;
    }
    real_swap(window);
    ++swap_number;
}
