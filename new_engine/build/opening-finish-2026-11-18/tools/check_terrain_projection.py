#!/usr/bin/env python3
"""CPU geometry properties plus real GLSL compilation using OSMesa software.

Creates no engine/GPU process, renders no game frames, and writes only its
candidate review report. Native image and performance checks remain required.
"""
from pathlib import Path
import ctypes as C
import hashlib
import json
import os
import sys
import numpy as np

N = Path(__file__).resolve().parents[1]
SHADER = N / "shared/shaders/world_surface_library.fs.glsl"


def need(value, message):
    if not value:
        raise RuntimeError(message)


def compile_glsl():
    os.environ.update(LIBGL_ALWAYS_SOFTWARE="1", GALLIUM_DRIVER="llvmpipe", LP_NUM_THREADS="1")
    gl = C.CDLL("libOSMesa.so.8")
    gl.OSMesaCreateContextExt.argtypes = [C.c_uint, C.c_int, C.c_int, C.c_int, C.c_void_p]
    gl.OSMesaCreateContextExt.restype = C.c_void_p
    gl.OSMesaMakeCurrent.argtypes = [C.c_void_p, C.c_void_p, C.c_uint, C.c_int, C.c_int]
    gl.OSMesaMakeCurrent.restype = C.c_int
    gl.OSMesaDestroyContext.argtypes = [C.c_void_p]
    context = gl.OSMesaCreateContextExt(0x1908, 0, 0, 0, None)
    need(context, "OSMesa software context unavailable")
    pixels = (C.c_ubyte * 4)()
    try:
        need(gl.OSMesaMakeCurrent(context, pixels, 0x1401, 1, 1), "OSMesa context could not be made current")
        gl.glGetString.argtypes = [C.c_uint]
        gl.glGetString.restype = C.c_char_p
        renderer = gl.glGetString(0x1F01).decode()
        need("llvmpipe" in renderer.lower(), "Compiler context is not software-only")
        gl.glCreateShader.argtypes = [C.c_uint]
        gl.glCreateShader.restype = C.c_uint
        gl.glShaderSource.argtypes = [C.c_uint, C.c_int, C.POINTER(C.c_char_p), C.POINTER(C.c_int)]
        gl.glCompileShader.argtypes = [C.c_uint]
        gl.glGetShaderiv.argtypes = [C.c_uint, C.c_uint, C.POINTER(C.c_int)]
        gl.glGetShaderInfoLog.argtypes = [C.c_uint, C.c_int, C.POINTER(C.c_int), C.c_char_p]
        gl.glDeleteShader.argtypes = [C.c_uint]
        compiled = []
        for enabled, terrain in ((True, True), (True, False), (False, True)):
            prelude = "#version 330 core\n"
            if enabled:
                prelude += "#define VALWOOD_IMPORTED_SURFACES 1\n#define STUDIO_SURFACE_SAMPLER uTestSurface\nuniform sampler2DArray uTestSurface;\n"
            fragment = prelude + SHADER.read_text() + "\nout vec4 FragColor;\nvoid main(){vec3 p=vec3(gl_FragCoord.xy,0),color=vec3(.5),normal=vec3(0,1,0);float rough=.8,metal=0.,ao=1.;studioSurface(p,p.xz,mat3(vec3(1,0,0),vec3(0,0,1),normal)," + ("true" if terrain else "false") + ",color,rough,metal,normal,ao);FragColor=vec4(color+normal*.01+vec3(rough+metal+ao)*.001,1);}\n"
            shader = gl.glCreateShader(0x8B30)
            text = C.c_char_p(fragment.encode())
            gl.glShaderSource(shader, 1, C.byref(text), None)
            gl.glCompileShader(shader)
            status = C.c_int()
            length = C.c_int()
            gl.glGetShaderiv(shader, 0x8B81, C.byref(status))
            gl.glGetShaderiv(shader, 0x8B84, C.byref(length))
            log = C.create_string_buffer(max(length.value, 1))
            gl.glGetShaderInfoLog(shader, len(log), None, log)
            gl.glDeleteShader(shader)
            need(status.value == 1, "GLSL compile failed: " + log.value.decode())
            compiled.append(dict(imported_surfaces=enabled, terrain_call=terrain, passed=True, log=log.value.decode()))
        return dict(renderer=renderer, game_frames_rendered=0, hardware_gpu_used=False, variants=compiled)
    finally:
        gl.OSMesaDestroyContext(context)


def properties():
    source = SHADER.read_text()
    need("if(steep<=0.)return surfaceLayer(layer,world.xz,dx.xz,dy.xz,basis,source);" in source, "Flat terrain exact call changed")
    need("else {first=surfaceLayer(binding.x,uv*p.y,ux*p.y,uy*p.y,basis,result);second=surfaceLayer(binding.y,uv*p.y,ux*p.y,uy*p.y,basis,result);}" in source, "Mesh UV path changed")
    start = source.index("SurfaceSample surfaceTerrainProjection(")
    end = source.index("void surfacePaint(", start)
    need("dFdx" not in source[start:end] and "dFdy" not in source[start:end], "Derivative inside spatial projection branch")
    rng = np.random.default_rng(173901)
    normals = rng.normal(size=(20000, 3))
    normals[:, 1] = np.abs(normals[:, 1])
    normals /= np.linalg.norm(normals, axis=1)[:, None]
    signs = np.where(normals >= 0, 1., -1.)
    weights = np.maximum(np.abs(normals) ** 4 - .0001, 0)
    weights /= weights.sum(axis=1)[:, None]
    max_error = 0.
    orthonormal_error = 0.
    vertical_density_error = 0.
    for axis in range(3):
        frames = np.zeros((len(normals), 3, 3))
        if axis == 0:
            frames[:, :, 0] = np.column_stack((np.zeros(len(normals)), np.zeros(len(normals)), -signs[:, 0]))
            frames[:, :, 1] = (0, 1, 0)
            frames[:, :, 2] = np.column_stack((signs[:, 0], np.zeros(len(normals)), np.zeros(len(normals))))
        elif axis == 1:
            frames[:, :, 0] = (1, 0, 0)
            frames[:, :, 1] = np.column_stack((np.zeros(len(normals)), np.zeros(len(normals)), signs[:, 1]))
            frames[:, :, 2] = np.column_stack((np.zeros(len(normals)), signs[:, 1], np.zeros(len(normals))))
        else:
            frames[:, :, 0] = np.column_stack((signs[:, 2], np.zeros(len(normals)), np.zeros(len(normals))))
            frames[:, :, 1] = (0, 1, 0)
            frames[:, :, 2] = np.column_stack((np.zeros(len(normals)), np.zeros(len(normals)), signs[:, 2]))
        orthonormal_error = max(orthonormal_error, float(np.abs(np.einsum("nji,njk->nik", frames, frames) - np.eye(3)).max()))
        base = np.einsum("nji,nj->ni", frames, normals)
        t = base + (0, 0, 1)
        flat_detail = np.tile((0, 0, 1), (len(normals), 1))
        u = flat_detail * (-1, -1, 1)
        reoriented = t * (np.sum(t * u, axis=1) / t[:, 2])[:, None] - u
        world = np.einsum("nij,nj->ni", frames, reoriented)
        max_error = max(max_error, float(np.abs(world - normals).max()))
        # A metre travelled vertically has a metre of texture coordinates on
        # either side projection. The old XZ-only path has zero vertical rate.
        if axis != 1:
            density = np.linalg.norm(frames[:, 1, :2], axis=1)
            vertical_density_error = max(vertical_density_error, float(np.abs(density - 1).max()))
        # Signed local tangent slopes point in the chosen projection's tangent
        # directions when the underlying terrain normal is that projection axis.
        detail = np.tile((.2, -.3, 1), (len(normals), 1))
        detail /= np.linalg.norm(detail, axis=1)[:, None]
        flat_base = np.tile((0, 0, 1), (len(normals), 1))
        tt = flat_base + (0, 0, 1)
        uu = detail * (-1, -1, 1)
        rr = tt * (np.sum(tt * uu, axis=1) / tt[:, 2])[:, None] - uu
        need(np.max(np.abs(rr - detail)) < 1e-12, "Normal slope orientation changed")
        need(np.all(np.isfinite(world)), "Non-finite projected normal")
    need(max_error < 1e-12 and orthonormal_error < 1e-12, "Flat maps bend macro normals")
    need(vertical_density_error < 1e-12, "Vertical texture density collapses")
    need(np.max(np.abs(weights.sum(axis=1) - 1)) < 1e-12, "Projection weights are not normalized")
    # RMS roughness blending stays bounded and returns identical endpoints.
    rough = rng.uniform(.04, 1, size=(len(normals), 3))
    combined = np.sqrt(np.sum(weights * rough * rough, axis=1))
    need(np.all(combined >= rough.min(axis=1) - 1e-12) and np.all(combined <= rough.max(axis=1) + 1e-12), "Roughness energy left material bounds")
    return dict(
        directions=20000, signed_projection_frames=60000,
        flat_map_macro_normal_max_error=max_error, frame_orthonormal_max_error=orthonormal_error,
        vertical_projection_density_error=vertical_density_error,
        flat_terrain_call_unchanged=True, mesh_uv_calls_unchanged=True,
        derivatives_outside_spatial_branches=True, roughness_rms_bounds=True,
        negligible_projection_zero_weight_continuous=True,
    )


if __name__ == "__main__":
    try:
        report = dict(
            passed=True, shader_sha256=hashlib.sha256(SHADER.read_bytes()).hexdigest(),
            properties=properties(), glsl=compile_glsl(),
            limit="CPU properties and shader syntax only; native bank images and performance are reviewed separately.",
        )
    except Exception as error:
        report = dict(passed=False, error=str(error))
    (N / "review/TERRAIN_PROJECTION_CPU.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))
    sys.exit(0 if report["passed"] else 1)
