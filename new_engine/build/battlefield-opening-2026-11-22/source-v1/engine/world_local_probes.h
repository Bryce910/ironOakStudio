#ifndef VALWOOD_WORLD_LOCAL_PROBES_H
#define VALWOOD_WORLD_LOCAL_PROBES_H

#include <cstddef>
#include <cstdint>

constexpr unsigned WORLD_LOCAL_PROBE_LIMIT=8;
struct WorldLocalProbeDef {
    double position[3]={};
    float box_min[3]={},box_max[3]={}; // absolute world metres, finite nonempty box
    bool interior=false;
};
struct WorldLocalProbeCapture {
    unsigned framebuffer=0,index=0,face=0;
    int side=0;
    double eye[3]={};
    float forward[3]={},up[3]={},right[3]={};
};
using WorldLocalProbeCaptureFn=bool (*)(const WorldLocalProbeCapture &,void *);
struct WorldLocalProbeStats {
    unsigned count=0,ready=0;
    int side=0;
    std::uint64_t bytes=0,cube_bytes=0,depth_bytes=0;
    std::uint64_t capture_calls=0,capture_faces=0,readbacks=0,bind_calls=0,failures=0;
};
struct WorldLocalProbes;

/* Render-thread GL3.3 API. Startup-only RGB16F cubemaps, max8*128², with
 * 64-sample GGX roughness levels matching the HDR environment prefilter. Shared
 * DEPTH24 capture target; one complete temporary raw cube/program/VAO is reused
 * during loading and deleted on every exit. Resident stats remain final cubes
 * plus depth; even loading peak cubes+raw+depth stay below8MiB.
 * Invalid definitions/count/side return nullptr without creating GL objects.
 */
WorldLocalProbes *world_local_probes_create(const WorldLocalProbeDef *,std::size_t count,int side=128);
/* One successful bake per module. Callback draws into capture.framebuffer at
 * side² with90-degree square projection and supplied OpenGL cubemap basis.
 * Render static scene/sky without sun disk, actors or particles; call bind
 * with enabled=false while capturing. No probe may sample its own target.
 * Module attaches COLOR0 and depth; output1 is discarded. Callback may change
 * GL state/FBO. Module rebinds its target for startup-only RGB float readback.
 * SH is integrated from raw linear capture before specular GGX convolution.
 * Framebuffer/viewport/renderbuffer/program/VAO, unit0 cube+sampler, render
 * enables/masks/depth/blend/cull/scissor parameters and pixelstore are restored.
 */
bool world_local_probes_capture(WorldLocalProbes *,WorldLocalProbeCaptureFn,void *);
/* Fixed cache for eight shader programs (terrain, scenery and live actors).
 * Register during loading, including after the shared shader epoch changes;
 * stale entries are reused on replacement. Bind can lazily register. No allocations/readbacks or
 * GL state queries after registration. Program must already be current.
 * Cube uniforms: uLocalProbe0/1 (units4/5), uLocalProbeCount,
 * uLocalProbeCenter[2],uLocalProbeBoxMin[2],uLocalProbeBoxMax[2],
 * uLocalProbeWeights(vec2),uLocalProbeMip(float),uLocalProbeLightScale(float).
 * Terrain may omit all cube uniforms. SH uniforms match world_terrain:
 * uLocalIrradiance[9],Weight,Bounds(xmin,zmin,xmax,zmax),Heights(minY,maxY),
 * plus uLocalIrradianceSecondary[9],SecondaryWeight,SecondaryBounds,
 * SecondaryHeights. Each coefficient bank retains its own spatial domain;
 * runtime weights sum to at most.40, avoiding a dominant-box boundary switch.
 * Chooses at most two containing camera domains; indoor domains take priority
 * over exterior domains. Disabled/outside/capture path writes zero weights.
 * After a successful bake, absent cube slots reuse a complete captured cube;
 * unbaked/capture paths bind zero, preventing target feedback. No extra texture.
 * Leaves texture unit0 active; it does not change the current program.
 */
bool world_local_probes_register_program(WorldLocalProbes *,unsigned program);
bool world_local_probes_bind(WorldLocalProbes *,unsigned program,const double camera[3],
                            bool enabled=true,float light_scale=1.f);
WorldLocalProbeStats world_local_probes_stats(const WorldLocalProbes *);
void world_local_probes_free(WorldLocalProbes *);

/* CPU-only public math for regression and offline inspection. RGB face order
 * is +X,-X,+Y,-Y,+Z,-Z, each row starts at OpenGL bottom. SH layout matches
 * the terrain shader: [1,y,z,x,xy,yz,3z²-1,xz,x²-y²]. add_face accumulates
 * radiance coefficients using exact per-texel cube solid angles. Caller zeroes
 * coefficients before the six faces. No allocation or GL calls.
 */
bool world_local_probe_face_basis(unsigned face,float forward[3],float up[3],float right[3]);
bool world_local_probe_add_face(unsigned face,int side,const float *rgb,std::size_t values,
                                double coefficients[9][3]);
void world_local_probe_diffuse(const double coefficients[9][3],const double normal[3],double rgb[3]);
std::uint64_t world_local_probe_cube_bytes(unsigned count,int side);

#endif
