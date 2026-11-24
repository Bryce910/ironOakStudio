#ifndef VALWOOD_WORLD_ANIMATION_RENDER_H
#define VALWOOD_WORLD_ANIMATION_RENDER_H
#include "engine/world_animation.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace world_animation_render {
// Unit16 belongs to the world local-light samplerCube. A samplerBuffer must
// use a distinct unit even when local-light shadows are disabled by a uniform.
constexpr unsigned instance_limit=128,palette_unit=17;
struct Instance {float x=0,y=0,z=0,scale=1,yaw=0;};
// Caller-known prior GL bindings; passing them avoids driver state queries in
// draw/upload. capture_bindings() is an explicit loading/test helper only.
struct Bindings {
    unsigned vao=0,array_buffer=0,texture_buffer=0,active_texture=0x84c0;
    std::array<unsigned,4> textures={},samplers={};
    unsigned palette_texture=0,palette_sampler=0;bool cull_face=false;
    // Explicit authored material graph for this draw, -1 disables it. This is
    // a draw request, not queried GL state; the uniform resets to -1 afterward.
    int studio_graph_index=-1;
    std::array<float,3> blood_wear={}; // coverage, stable UV seed, wet fraction
    // Empty by default. Hide only this contiguous primitive interval; FP
    // lining primitives after held gear remain visible. No geometry changes.
    unsigned hidden_primitive_begin=0,hidden_primitive_end=0;
};
Bindings capture_bindings();
struct Stats {
    unsigned capacity=0,instances=0,joints=0,materials=0,morph_mode=0;
    std::uint64_t geometry_bytes=0,palette_bytes=0,morph_bytes=0,instance_bytes=0,reference_texture_bytes=0;
    std::uint64_t uploads=0,draws=0,failures=0;
};
struct Renderer;
// GL3.3 owner/current-context only. Copies bounded immutable draw definitions;
// asset lifetime need not exceed create(). No uploaded source is overwritten.
// Acquisition failures release all partial buffers/cache references.
Renderer *create(const world_animation::Asset &,unsigned capacity,std::string &error,
                 std::uint64_t texture_budget_bytes=256ull*1024*1024);
void destroy(Renderer *);
// Loading/shader-replacement boundary only: validates/caches all locations.
// Draw fails closed after program epoch change until warm() is called again.
bool warm(Renderer *,unsigned linked_program,std::string &error);
// Finite input/capacity/pose checks precede any GL changes. Two preallocated
// banks commit only after complete pose+instance upload; old active data survive
// a failed upload. No heap allocation, IO, resizing or uniform queries.
bool upload(Renderer *,const world_animation::Pose *,const Instance *,std::size_t count,
            const Bindings &restore,std::string &error);
// Caller binds linked_program and sets VP/offset/world lighting. Material state
// is set per primitive; reactive confidence resets afterward. This does not
// bind another program or alter framebuffer/depth/blend. Owned texture/VAO/
// sampler/culling bindings restore to the explicitly supplied state.
bool draw(Renderer *,unsigned linked_program,const Bindings &restore,std::string &error);
Stats stats(const Renderer *);
// Palette texture committed by the previous upload (last frame's pose), or0.
// Mode1 shares one texture: add previous_palette_offset() to the current
// instance palette base for old joints/weights; the immutable morph tail stays.
unsigned previous_palette(const Renderer *);
int previous_palette_offset(const Renderer *);
// Read-only loading/test diagnostics, never queried in the drawing path.
struct MaterialTextures {std::array<unsigned,4> maps={};};
bool material_textures(const Renderer *,unsigned material,MaterialTextures &);
}
#endif
