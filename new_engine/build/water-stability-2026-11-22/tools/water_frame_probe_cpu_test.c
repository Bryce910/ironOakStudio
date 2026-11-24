/* CPU-only GL/SDL mock: does not create a window/context or call a graphics driver. */
#define _GNU_SOURCE
#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_opengl_glext.h>
#include <assert.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static void *mock_dlsym(void *, const char *);
#define dlsym mock_dlsym
#include "water_frame_probe.c"
#undef dlsym

static GLint read_fbo = 7, default_buffer = GL_FRONT_LEFT, attachment_buffer = GL_COLOR_ATTACHMENT1;
static GLint pbo = 99;
static GLint packs[8] = {8, 31, 2, 3, 17, 4, 1, 1};
static const GLenum pack_names[8] = {GL_PACK_ALIGNMENT, GL_PACK_ROW_LENGTH, GL_PACK_SKIP_ROWS,
    GL_PACK_SKIP_PIXELS, GL_PACK_IMAGE_HEIGHT, GL_PACK_SKIP_IMAGES, GL_PACK_SWAP_BYTES, GL_PACK_LSB_FIRST};
static unsigned read_calls, swap_calls, matrix_calls;
static const GLfloat *matrix_address;
static void mock_swap(SDL_Window *window) { assert(window == (SDL_Window *)1); ++swap_calls; }
static void APIENTRY mock_matrix(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) {
    assert(location == 12 || location == 13); assert(count == 1); assert(transpose == GL_FALSE);
    assert(value == matrix_address); ++matrix_calls;
}
static void *mock_dlsym(void *handle, const char *name) {
    assert(handle == RTLD_NEXT);
    if (!strcmp(name, "glUniformMatrix4fv")) return (void *)mock_matrix;
    if (!strcmp(name, "SDL_GL_SwapWindow")) return (void *)mock_swap;
    abort();
}
SDL_GLContext SDL_GL_GetCurrentContext(void) { return (SDL_GLContext)1; }
void SDL_GL_GetDrawableSize(SDL_Window *window, int *w, int *h) { assert(window == (SDL_Window *)1); *w = 2; *h = 2; }
void APIENTRY glGetIntegerv(GLenum name, GLint *value) {
    for (unsigned i = 0; i < 8; ++i) if (name == pack_names[i]) { *value = packs[i]; return; }
    switch (name) {
        case GL_PIXEL_PACK_BUFFER_BINDING: *value = pbo; return;
        case GL_READ_FRAMEBUFFER_BINDING: *value = read_fbo; return;
        case GL_READ_BUFFER: *value = read_fbo ? attachment_buffer : default_buffer; return;
        case GL_CURRENT_PROGRAM: *value = 37; return;
    }
    abort();
}
GLint APIENTRY glGetUniformLocation(GLuint program, const GLchar *name) {
    assert(program == 37); assert(!strcmp(name, "uReflectionVP")); return 12;
}
void APIENTRY glGetBooleanv(GLenum name, GLboolean *value) { assert(name == GL_DOUBLEBUFFER); *value = GL_TRUE; }
void APIENTRY glBindFramebuffer(GLenum target, GLuint fbo) { assert(target == GL_READ_FRAMEBUFFER); assert(fbo == 0 || fbo == 7); read_fbo = (GLint)fbo; }
void APIENTRY glReadBuffer(GLenum buffer) { if (read_fbo) attachment_buffer = (GLint)buffer; else default_buffer = (GLint)buffer; }
void APIENTRY glBindBuffer(GLenum target, GLuint buffer) { assert(target == GL_PIXEL_PACK_BUFFER); pbo = (GLint)buffer; }
void APIENTRY glPixelStorei(GLenum name, GLint value) {
    for (unsigned i = 0; i < 8; ++i) if (name == pack_names[i]) { packs[i] = value; return; }
    abort();
}
void APIENTRY glReadPixels(GLint x, GLint y, GLsizei w, GLsizei h, GLenum format, GLenum type, void *data) {
    assert(x == 0 && y == 0 && w == 2 && h == 2 && format == GL_RGB && type == GL_UNSIGNED_BYTE);
    assert(read_fbo == 0 && default_buffer == GL_BACK && pbo == 0);
    assert(packs[0] == 1);
    for (unsigned i = 1; i < 8; ++i) assert(packs[i] == 0);
    for (unsigned i = 0; i < 12; ++i) ((unsigned char *)data)[i] = (unsigned char)i;
    ++read_calls;
}
static void assert_restored(void) {
    const GLint expected[8] = {8,31,2,3,17,4,1,1};
    assert(!memcmp(packs, expected, sizeof expected));
    assert(pbo == 99 && read_fbo == 7 && default_buffer == GL_FRONT_LEFT && attachment_buffer == GL_COLOR_ATTACHMENT1);
}
int main(int argc, char **argv) {
    assert(argc == 2);
    char prefix_name[4096], gate_name[4096];
    snprintf(prefix_name, sizeof prefix_name, "%s/frame", argv[1]);
    snprintf(gate_name, sizeof gate_name, "%s/capture.ready", argv[1]);
    assert(!setenv("VALWOOD_WATER_CAPTURE_PREFIX", prefix_name, 1));
    assert(!setenv("VALWOOD_WATER_CAPTURE_GATE", gate_name, 1));
    GLfloat matrix[16]; for (unsigned i=0;i<16;++i) matrix[i]=(GLfloat)i+.125f;
    matrix_address = matrix;
    glUniformMatrix4fv(12, 1, GL_FALSE, matrix);
    SDL_GL_SwapWindow((SDL_Window *)1);
    assert(!read_calls); assert_restored();
    FILE *ready = fopen(gate_name, "wb"); assert(ready); fclose(ready);
    for (unsigned i=0; i<40; ++i) {
        glUniformMatrix4fv(13, 1, GL_FALSE, matrix); /* Other uniforms pass through without logging. */
        glUniformMatrix4fv(12, 1, GL_FALSE, matrix);
        SDL_GL_SwapWindow((SDL_Window *)1);
        assert_restored();
    }
    assert(read_calls == 32 && swap_calls == 41 && matrix_calls == 81 && captured == 32 && finished);
    printf("PASS: gate; 32 consecutive reads; nonmatching uniforms forwarded; all matrices forwarded unchanged; full PACK/PBO/READ FBO/read-buffer restoration after every swap.\n");
    return 0;
}
