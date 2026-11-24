# Water refinement implementation

All production changes are in `build/collision-water-2026-11-06/source-v3` (production water bytes unchanged from v2); accepted parent snapshots and user projects were not changed by this agent. No new runtime compilation unit, document format, external asset or texture was introduced.

## Shared geometry and terrain

`engine/world_river_network.h` builds a bounded 16 m segment index. Rounded channel signed distances are clipped on the same 2 m world grid as the terrain, with one non-overlapping triangle partition for the entire river union. End caps, adjacent segments, bends, tributaries and separate crossing ribbons cannot double-draw. Mesh ranges are 64 m culling chunks, so their count no longer equals authored river count. The retained maximum design of 16 rivers, each 1,024 m long and 32 m wide, builds 809,760 vertices / 21.62 MiB in approximately 0.31 CPU seconds here. The mesh has an explicit 1.5 million vertex ceiling and atomic preparation failure.

CPU water sampling uses the same world-grid diagonal and interpolation as the drawn mesh. Junction heights and tangent directions blend continuously. Each sloping segment flows downhill even after advanced numeric Y edits; flat segments retain authored direction. Channel beds use the union of submerged cuts, and a tributary's shoulder cannot fill the earlier main channel. Road grading retains its own ordered behavior. Existing edit serialization, derived-field reconstruction, and pond basin generation stay in place.

The new endpoint height helper samples existing river water, rather than its excavated bed. The beginner River footprint permits other River ribbons while preserving roads, fixed shores/ocean, existing ponds, imported ground, model roots, foundations and NPC/travel paths. The river-specific preflight must not use `world_atlas_local_water` as an unconditional obstruction: that function includes live authored rivers and previously prevented a real junction.

## Rendering

The river vertex carries direction XZ, bank distance and downhill grade. Waves and foam advect along that authored direction; sloping surfaces use their geometric normal. Shore contact uses the opaque color/depth snapshot, with optical coverage fading over 0.008–0.16 m. Refraction continues to validate every bilinear footprint depth to keep above-water banks from leaking into the water.

Bank foam has broken lacing, downstream transport, and grade-driven riffles. Small ponds use substantially calmer waves and only a trace of foam. Far wave components and small foam detail are filtered by the pixel footprint. The existing HDR sky/reflections, cloud and torch lighting, weather, imported material graphs and alpha reactivity convention remain present. The alpha channel is not used to enable conventional transparency.

`uTanHalfFov` replaces a hard-coded camera FOV in opaque-depth reconstruction. Viewer integration must set it to `tan(cam.fov * .5f)` and reset `uRiverSurface` before ocean/pool draws. The optional radius on river `visible/nearby` allows the viewer's ordinary custom-water distance to control all authored water consistently.

## Shader application

All four retained seed water shader pairs were proved byte-identical before this pass:

- VS: `f3960fcefdce24f706ace8de6c57258cfc9474504cbcaae7aafe4f02cfddc304`
- FS: `db1568d8cba40836a1307f88e3f886902552fca9d461804901cb4e138246cb25`

`rebase_water_shaders.py` plus `water_shader_patch.json` applies only guarded water hunks. It supports idempotence and reversal, preserves unrelated custom additions, and rejects conflicting edits before touching either shader. It must be applied in the delivery bootstrap's existing transaction/backups. Other art shaders must never be replaced from a different seed. The helper is independently tested against all four seeds.

## Evidence

- `CPU_RESULTS.json`: actual production geometry, terrain, pond, landscape and dry-root regressions; 191,521 checks across five executables.
- `SHADER_PATCH_CPU_RESULTS.json`: 99 seed/rebase/optics contracts. This is not a GLSL compiler or a visual acceptance claim.
- `JUNCTION_UI_CPU_RESULTS.json`: actual editor helper method bodies compiled against production geometry, document, protection and terrain code. Controlled fixed-atlas callbacks isolate River-on-River acceptance, water-level snap and retained protected overlaps; creation and serialized reopening are also exercised.

The parent agent owns real editor input, graphics compile/render inspection, Linux/Windows builds and installation. This CPU-only agent did not acquire the GPU, close the user's editor or change the live workcopy.
