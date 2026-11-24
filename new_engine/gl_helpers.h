/*
 * gl_helpers.h  –  Simple OpenGL 3.3 utility wrappers
 *
 * Instead of writing 20 lines of boilerplate for every shader/buffer/texture,
 * use these helpers to keep your code readable.
 *
 * Example:
 *   GLuint prog = GLH_program(vert_src, frag_src);
 *   GLH_mat4(prog, "uProj", proj.m);
 *   GLH_vec3(prog, "uLightPos", 5, 10, 5);
 *   GLH_float(prog, "uTime", t);
 *
 *   GLuint vao = GLH_mesh(verts, sizeof(verts), inds, sizeof(inds),
 *                          sizeof(Vertex),
 *                          (GLH_Attrib[]){
 *                              {0, 3, GL_FLOAT, 0, offsetof(Vertex,pos)},
 *                              {1, 3, GL_FLOAT, 0, offsetof(Vertex,normal)},
 *                              {2, 2, GL_FLOAT, 0, offsetof(Vertex,uv)},
 *                          }, 3);
 */

#pragma once
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_opengl_glext.h>

/* ─── Attribute descriptor ────────────────────────────────────── */
typedef struct {
    int    loc;     /* layout location */
    int    n;       /* number of components (1-4) */
    GLenum type;    /* GL_FLOAT, GL_INT, etc */
    int    norm;    /* GL_TRUE / GL_FALSE */
    size_t offset;  /* offsetof(Struct, field) */
} GLH_Attrib;

/* ═══════════════════════════════════════════════════════════════
   SHADER COMPILATION
   ═══════════════════════════════════════════════════════════════ */

/* Compile a single shader stage, print errors if any */
static inline GLuint glh_compile(GLenum type, const char *src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    GLint ok;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(s, sizeof(log), NULL, log);
        fprintf(stderr, "[GL] Shader compile error (%s):\n%s\n",
                type==GL_VERTEX_SHADER?"vert":"frag", log);
    }
    return s;
}

/* Load a GLSL source file, resolving #pragma include "rel/path" directives.
   Returns a heap-allocated string the caller must free(), or NULL on error. */
static char *glsl_load(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) { fprintf(stderr, "[GLSL] Cannot open: %s\n", path); return NULL; }
    char dir[512] = "";
    const char *sl = strrchr(path, '/');
    if (sl) { size_t dl = (size_t)(sl - path + 1); if (dl < sizeof(dir)) { memcpy(dir, path, dl); dir[dl] = '\0'; } }
    size_t cap = 8192, len = 0;
    char *buf = (char*)malloc(cap); if (!buf) { fclose(f); return NULL; } buf[0] = '\0';
    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        const char *p = line; while (*p == ' ' || *p == '\t') p++;
        if (strncmp(p, "#pragma include", 15) == 0) {
            const char *q = strchr(p, '"'); if (q) {
                const char *e = strchr(q+1, '"'); if (e) {
                    char ir[512], ia[512]; size_t il = (size_t)(e-q-1);
                    if (il < sizeof(ir)) {
                        memcpy(ir, q+1, il); ir[il] = '\0';
                        snprintf(ia, sizeof(ia), "%s%s", dir, ir);
                        char *inc = glsl_load(ia);
                        if (inc) {
                            size_t il2 = strlen(inc);
                            while (len + il2 + 2 > cap) { cap *= 2; buf = (char*)realloc(buf, cap); }
                            memcpy(buf+len, inc, il2); len += il2; buf[len++] = '\n'; buf[len] = '\0';
                            free(inc);
                        }
                        continue;
                    }
                }
            }
        }
        size_t ll = strlen(line);
        while (len + ll + 2 > cap) { cap *= 2; buf = (char*)realloc(buf, cap); }
        memcpy(buf+len, line, ll); len += ll; buf[len] = '\0';
    }
    fclose(f); return buf;
}

static inline GLuint GLH_program(const char *vert, const char *frag);

/* Build a shader program from .glsl files (with #pragma include support) */
static inline GLuint GLH_program_f(const char *vs_path, const char *fs_path) {
    char *vs = glsl_load(vs_path);
    char *fs = glsl_load(fs_path);
    GLuint prog = 0;
    if (vs && fs) {
        prog = GLH_program(vs, fs);
    } else {
        fprintf(stderr, "[GLSL] Failed to load: %s / %s\n", vs_path, fs_path);
        prog = GLH_program(
            "#version 330 core\nvoid main(){gl_Position=vec4(0);}\n",
            "#version 330 core\nout vec4 F;void main(){F=vec4(1,0,1,1);}\n");
    }
    free(vs); free(fs); return prog;
}

/* Build a complete shader program from vertex + fragment source strings */
static inline GLuint GLH_program(const char *vert, const char *frag) {
    GLuint vs = glh_compile(GL_VERTEX_SHADER,   vert);
    GLuint fs = glh_compile(GL_FRAGMENT_SHADER, frag);
    GLuint p  = glCreateProgram();
    glAttachShader(p, vs);
    glAttachShader(p, fs);
    glLinkProgram(p);
    GLint ok;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(p, sizeof(log), NULL, log);
        fprintf(stderr, "[GL] Program link error:\n%s\n", log);
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    return p;
}

/* ═══════════════════════════════════════════════════════════════
   UNIFORM SETTERS  (look up + set in one call)
   ═══════════════════════════════════════════════════════════════ */

static inline void GLH_mat4(GLuint prog, const char *name, const float *m) {
    glUniformMatrix4fv(glGetUniformLocation(prog, name), 1, GL_FALSE, m);
}
static inline void GLH_vec4(GLuint prog, const char *name, float x, float y, float z, float w) {
    glUniform4f(glGetUniformLocation(prog, name), x, y, z, w);
}
static inline void GLH_vec3(GLuint prog, const char *name, float x, float y, float z) {
    glUniform3f(glGetUniformLocation(prog, name), x, y, z);
}
static inline void GLH_vec2(GLuint prog, const char *name, float x, float y) {
    glUniform2f(glGetUniformLocation(prog, name), x, y);
}
static inline void GLH_float(GLuint prog, const char *name, float v) {
    glUniform1f(glGetUniformLocation(prog, name), v);
}
static inline void GLH_int(GLuint prog, const char *name, int v) {
    glUniform1i(glGetUniformLocation(prog, name), v);
}
static inline void GLH_bool(GLuint prog, const char *name, int v) {
    glUniform1i(glGetUniformLocation(prog, name), v);
}

/* Set a mat4 array uniform, e.g. uBones[0..n] */
static inline void GLH_mat4_array(GLuint prog, const char *name,
                                   int count, const float *m) {
    glUniformMatrix4fv(glGetUniformLocation(prog, name), count, GL_FALSE, m);
}

/* Set a vec3 array, e.g. uLightPos[0..n] */
static inline void GLH_vec3_array(GLuint prog, const char *name,
                                   int count, const float *v) {
    glUniform3fv(glGetUniformLocation(prog, name), count, v);
}

/* Set a float array */
static inline void GLH_float_array(GLuint prog, const char *name,
                                    int count, const float *v) {
    glUniform1fv(glGetUniformLocation(prog, name), count, v);
}

/* ═══════════════════════════════════════════════════════════════
   MESH / BUFFER CREATION
   ═══════════════════════════════════════════════════════════════ */

/*
 * Create a VAO with interleaved vertex data + optional index buffer.
 *
 * vdata  - vertex array (raw floats/bytes)
 * vsize  - total byte size of vertex array
 * idata  - index array (uint32_t*), or NULL for non-indexed
 * isize  - total byte size of index array
 * stride - sizeof(YourVertexStruct)
 * attrs  - array of GLH_Attrib descriptors
 * nattrs - number of descriptors
 *
 * Returns VAO handle. Also creates VBO (and IBO if idata != NULL).
 */
static inline GLuint GLH_mesh_full(const void *vdata, size_t vsize,
                                   const void *idata, size_t isize,
                                   size_t stride,
                                   const GLH_Attrib *attrs, int nattrs,
                                   GLuint *out_vbo, GLuint *out_ibo)
{
    GLuint vao, vbo;
    GLuint ibo = 0;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)vsize, vdata,
                 vdata ? GL_STATIC_DRAW : GL_DYNAMIC_DRAW);

    if (idata) {
        glGenBuffers(1, &ibo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)isize, idata, GL_STATIC_DRAW);
    }

    for (int i = 0; i < nattrs; i++) {
        const GLH_Attrib *a = &attrs[i];
        glEnableVertexAttribArray(a->loc);
        if (a->type == GL_INT || a->type == GL_UNSIGNED_INT) {
            glVertexAttribIPointer(a->loc, a->n, a->type,
                                   (GLsizei)stride, (void*)a->offset);
        } else {
            glVertexAttribPointer(a->loc, a->n, a->type,
                                  (GLubyte)a->norm, (GLsizei)stride,
                                  (void*)a->offset);
        }
    }

    glBindVertexArray(0);
    if (out_vbo) *out_vbo = vbo;
    if (out_ibo) *out_ibo = ibo;
    return vao;
}

static inline GLuint GLH_mesh(const void *vdata, size_t vsize,
                              const void *idata, size_t isize,
                              size_t stride,
                              const GLH_Attrib *attrs, int nattrs)
{
    return GLH_mesh_full(vdata, vsize, idata, isize, stride, attrs, nattrs, NULL, NULL);
}

/* Create a dynamic VAO (for particles, HUD quads etc.) — pass NULL for vdata */
static inline GLuint GLH_mesh_dynamic(size_t max_vsize, size_t stride,
                                       const GLH_Attrib *attrs, int nattrs)
{
    return GLH_mesh(NULL, max_vsize, NULL, 0, stride, attrs, nattrs);
}

/* Update the VBO data of a dynamic mesh (binds VAO first) */
static inline void GLH_mesh_update(GLuint vao, const void *data, size_t size) {
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, 0); /* re-bind the buffer implicitly via VAO */
    /* We need the VBO id — store it right after the VAO in a simple wrapper.
     * Workaround: query currently bound VBO via vertex attrib pointer. */
    /* Simpler: caller manages their own VBO for dynamic meshes */
    (void)vao; (void)data; (void)size;
    /* Use GLH_vbo_update() instead — see below */
}

/* For dynamic meshes, manage VBO manually: */
static inline GLuint GLH_vbo(size_t max_size) {
    GLuint vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)max_size, NULL, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    return vbo;
}
static inline void GLH_vbo_update(GLuint vbo, const void *data, size_t size) {
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)size, data);
}

/* ═══════════════════════════════════════════════════════════════
   TEXTURE LOADING
   ═══════════════════════════════════════════════════════════════ */

/* Upload raw pixel data to a 2D texture */
static inline uint8_t *glh_prepare_rgba_for_mips(const uint8_t *src, int w, int h, int ch) {
    if (!src || ch != 4 || w <= 0 || h <= 0) return NULL;
    size_t count = (size_t)w * (size_t)h * 4;
    uint8_t *tmp = (uint8_t*)malloc(count);
    if (!tmp) return NULL;
    memcpy(tmp, src, count);

    /* Bleed neighboring opaque RGB into transparent texels to avoid atlas/mipmap halos. */
    for (int pass = 0; pass < 1; pass++) {
        int changed = 0;
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                size_t i = ((size_t)y * (size_t)w + (size_t)x) * 4;
                if (tmp[i + 3] != 0) continue;
                int r = 0, g = 0, b = 0, n = 0;
                static const int off[8][2] = {
                    {-1,0},{1,0},{0,-1},{0,1},{-1,-1},{1,-1},{-1,1},{1,1}
                };
                for (int k = 0; k < 8; k++) {
                    int nx = x + off[k][0], ny = y + off[k][1];
                    if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
                    size_t ni = ((size_t)ny * (size_t)w + (size_t)nx) * 4;
                    if (tmp[ni + 3] == 0) continue;
                    r += tmp[ni + 0];
                    g += tmp[ni + 1];
                    b += tmp[ni + 2];
                    n++;
                }
                if (n > 0) {
                    tmp[i + 0] = (uint8_t)(r / n);
                    tmp[i + 1] = (uint8_t)(g / n);
                    tmp[i + 2] = (uint8_t)(b / n);
                    changed = 1;
                }
            }
        }
        if (!changed) break;
    }
    return tmp;
}

static inline GLuint GLH_texture_pixels(const uint8_t *px, int w, int h, int ch) {
    if (!px) return 0;
    GLuint id;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    GLenum fmt = (ch == 4) ? GL_RGBA : GL_RGB;
    uint8_t *prepared = glh_prepare_rgba_for_mips(px, w, h, ch);
    glTexImage2D(GL_TEXTURE_2D, 0, (GLint)fmt, w, h, 0, fmt, GL_UNSIGNED_BYTE, prepared ? prepared : px);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    free(prepared);
    return id;
}

/* Upload albedo pixels as sRGB so lighting happens in linear space */
static inline GLuint GLH_texture_pixels_srgb(const uint8_t *px, int w, int h, int ch) {
    if (!px) return 0;
    GLuint id;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    GLenum fmt = (ch == 4) ? GL_RGBA : GL_RGB;
    GLenum internal = (ch == 4) ? GL_SRGB8_ALPHA8 : GL_SRGB8;
    uint8_t *prepared = glh_prepare_rgba_for_mips(px, w, h, ch);
    glTexImage2D(GL_TEXTURE_2D, 0, (GLint)internal, w, h, 0, fmt, GL_UNSIGNED_BYTE, prepared ? prepared : px);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    free(prepared);
    return id;
}

/* Load a texture from a file path */
static inline GLuint GLH_texture_file(const char *path) {
    int w, h, ch;
    stbi_set_flip_vertically_on_load(0);
    uint8_t *px = stbi_load(path, &w, &h, &ch, 4);
    if (!px) {
        fprintf(stderr, "[GL] Failed to load texture: %s\n", path);
        return 0;
    }
    GLuint id = GLH_texture_pixels(px, w, h, 4);
    stbi_image_free(px);
    printf("[GL] Texture loaded: %s (%dx%d ch=%d)\n", path, w, h, ch);
    return id;
}

/* Load an albedo texture from a file path as sRGB */
static inline GLuint GLH_texture_file_srgb(const char *path) {
    int w, h, ch;
    stbi_set_flip_vertically_on_load(0);
    uint8_t *px = stbi_load(path, &w, &h, &ch, 4);
    if (!px) {
        fprintf(stderr, "[GL] Failed to load texture: %s\n", path);
        return 0;
    }
    GLuint id = GLH_texture_pixels_srgb(px, w, h, 4);
    stbi_image_free(px);
    printf("[GL] Texture loaded sRGB: %s (%dx%d ch=%d)\n", path, w, h, ch);
    return id;
}

/* Load texture from memory (embedded PNG/JPG bytes) */
static inline GLuint GLH_texture_mem(const void *data, int size) {
    if (!data || size < 4) return 0;
    int w, h, ch;
    uint8_t *px = stbi_load_from_memory((const uint8_t*)data, size, &w, &h, &ch, 4);
    if (!px) return 0;
    GLuint id = GLH_texture_pixels(px, w, h, 4);
    stbi_image_free(px);
    return id;
}

/* Load embedded albedo texture bytes as sRGB */
static inline GLuint GLH_texture_mem_srgb(const void *data, int size) {
    if (!data || size < 4) return 0;
    int w, h, ch;
    uint8_t *px = stbi_load_from_memory((const uint8_t*)data, size, &w, &h, &ch, 4);
    if (!px) return 0;
    GLuint id = GLH_texture_pixels_srgb(px, w, h, 4);
    stbi_image_free(px);
    printf("[GL] Embedded sRGB texture (%dx%d ch=%d size=%d)\n", w, h, ch, size);
    return id;
}

/* Bind a texture to a slot and set the sampler uniform */
static inline void GLH_bind_texture(GLuint prog, const char *name, GLuint tex, int slot) {
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_2D, tex);
    GLH_int(prog, name, slot);
}

/* ═══════════════════════════════════════════════════════════════
   DRAW HELPERS
   ═══════════════════════════════════════════════════════════════ */

/* Use program + set projection + view in one call */
static inline void GLH_begin(GLuint prog, const float *proj, const float *view) {
    glUseProgram(prog);
    if (proj) GLH_mat4(prog, "uProj", proj);
    if (view) GLH_mat4(prog, "uView", view);
}

/* Draw an indexed VAO */
static inline void GLH_draw_indexed(GLuint vao, GLsizei count) {
    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

/* Draw an array VAO */
static inline void GLH_draw(GLuint vao, GLenum mode, GLsizei count) {
    glBindVertexArray(vao);
    glDrawArrays(mode, 0, count);
    glBindVertexArray(0);
}
