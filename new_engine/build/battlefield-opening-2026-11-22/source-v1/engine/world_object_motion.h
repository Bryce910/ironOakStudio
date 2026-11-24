#ifndef VALWOOD_WORLD_OBJECT_MOTION_H
#define VALWOOD_WORLD_OBJECT_MOTION_H

/* GL3.3 per-object motion buffer for temporal AA. An RGBA16F image at scene
 * size, cleared to zero each frame; selected actors are redrawn into it with
 * the scene depth attached read-only (LEQUAL, no depth writes), writing their
 * previous-NDC offset relative to static reprojection (a = 1). Everything
 * not redrawn keeps camera-only reprojection.
 * begin() binds the program and FBO and sets depth/cull/blend state;
 * end() restores depth function, depth test and polygon offset. */
struct WorldObjectMotion;
WorldObjectMotion *world_object_motion_create(const char *vertex,const char *fragment);
unsigned world_object_motion_program(const WorldObjectMotion *);
bool world_object_motion_begin(WorldObjectMotion *,int width,int height,unsigned scene_depth);
void world_object_motion_end(WorldObjectMotion *);
unsigned world_object_motion_texture(const WorldObjectMotion *);
void world_object_motion_free(WorldObjectMotion *);

#endif
