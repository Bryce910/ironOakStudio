#ifndef ENGINE_ASSET_SYSTEM_H
#define ENGINE_ASSET_SYSTEM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <SDL2/SDL_opengl.h>

#include "ufbx.h"

void ufbx_string_copy(char *dst,size_t cap,ufbx_string str);
void ufbx_string_to_cstr(char *dst,size_t cap,ufbx_string str);
void path_normalize_slashes(char *path);
bool path_has_extension(const char *path);
const char *path_basename_ptr(const char *path);

bool asset_resolve_path_copy(char *dst,size_t cap,const char *path);
uint8_t *asset_stbi_load(const char *path,int *w,int *h,int *ch,int desired_channels);
void texture_path_resolve(char *dst,size_t cap,const char *model_path,ufbx_string rel);

GLuint texture_file_try_candidates_mode(const char *path,bool srgb);
GLuint texture_file_try_candidates(const char *path);
GLuint try_texture_candidates(const char *const *candidates,size_t count);

bool path_contains_ci(const char *path,const char *needle);
bool is_direct_knight_asset(const char *path);
bool is_knight_copy_asset(const char *path);

#endif
