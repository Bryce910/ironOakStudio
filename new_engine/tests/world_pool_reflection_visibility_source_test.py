#!/usr/bin/env python3
"""Keep the CPU clip proof tied to the real water geometry and its consumers."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def tokens(source: str) -> str:
    source = re.sub(r"/\*.*?\*/|//[^\n]*", "", source, flags=re.S)
    return re.sub(r"\s+", "", source)


# This is the actual authoritative vertex program, not a duplicate material or
# visibility calculation. New vertex displacement/coordinate/clip semantics
# require updating the proof instead of silently invalidating its envelope.
expected = """#version 330 core
uniform mat4 uVP;
uniform vec3 uOffset;
uniform float uSize,uWaterLevel;
uniform bool uLocalPool;
uniform vec2 uOrigin;
uniform vec4 uPoolBounds;
out vec3 vRelative,vPos;
void main() {
    vec2 q[6]=vec2[6](vec2(-1,-1),vec2(1,-1),vec2(1,1),vec2(-1,-1),vec2(1,1),vec2(-1,1));
    vec2 xz=q[gl_VertexID]*uSize;
    if(uLocalPool)xz=uPoolBounds.xy-uOrigin+q[gl_VertexID]/max(uPoolBounds.zw,vec2(.000001));
    vec3 p=vec3(xz.x,uWaterLevel,xz.y);
    vRelative=p+uOffset;vPos=p;gl_Position=uVP*vec4(vRelative,1);
}
"""
assert tokens((ROOT / "shaders/world_water.vs.glsl").read_text()) == tokens(expected), "Water VS geometry changed: review the pool clip proof."
viewer = tokens((ROOT / "engine/world_viewer.cpp").read_text())
for statement in (
    'water.m4("uVP",vp);',
    'water.v3("uOffset",{0,float(-cam.y),0});',
    'water.i("uHasReflection",reflect_water&&water_level==0?1:0);',
    'water.i("uHasReflection",reflect_water&&water_level==float(pool->surface_y)?1:0);',
    'water.i("uHasReflection",reflect_water&&water_level==float(WORLD_CASTLE_CREEK_SURFACE_Y)?1:0);',
    'constboolwater_scene_ready=(reflect_water||draw_pool||draw_creek)&&world_water_scene_capture(water_scene,target.fbo,target.w,target.h);',
):
    assert statement in viewer, "Reflection consumer/snapshot ownership changed: review pool-only skipping."
assert "glClipControl(" not in viewer and "glEnable(GL_DEPTH_CLAMP)" not in viewer, "Main clip convention changed: review pool clip proof."
print("PASS pool reflection source contract: actual six-vertex water quad/default clip volume, three exclusive reflection owners and unchanged opaque-snapshot consumers.")
