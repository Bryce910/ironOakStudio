# The fifteen requested capabilities: implemented scope

This is a capability map for the current custom engine, not an assertion of parity with a mature commercial engine. Open Studio with F11. See USER-GUIDE.md for the new controls and STATUS.md for test evidence.

| Request | Where / actual behavior | Scope limit |
| --- | --- | --- |
| Landscape customization | Terrain / Advanced terrain: raise, lower, flatten, smooth, local height authoring and bounded hydraulic/thermal erosion; road/river grades | Bounded edited tiles and protected foundations; not an unlimited terrain DCC |
| Material blending | Surfaces: two-layer/source masks by noise, height, slope or constant; terrain stamp/erase | Eight reusable layers; 64 derived paint tiles |
| Clouds/fog | Atmosphere and quality controls; volumetric clouds/fog and canopy-specific mist | Tuned bounded volumetric quality; final walking tests assess performance separately |
| Texture quality | Source PBR preservation, role/color-space validation, DirectX normal convention, immutable imports, variance-aware normal mips | Imported surface filter is 1020px; source detail and model texel density still constrain results. Historical middle-LOD facade blend remains unchanged |
| Light types | Atmosphere sun/sky/HDR; Lights point/spot, colors, power, range, cone and flicker; local cached radiance probes | Point/spot share an eight-light contribution budget and do not add independent shadow maps; probes bake during loading |
| Particles | Particles editor for existing fire/smoke/dust appearance/emission; wind-field integration | Existing effect families; not an arbitrary GPU particle graph |
| Foliage | Density masks for grass/trees/rocks/ferns; model placement/scatter and alias culling/LODs | Masks suppress procedural roots; model scatter adds actual selected models |
| HDRI / skybox | Atmosphere / Assets: Radiance HDR import, rotation, intensity, blend; diffuse/specular environment and visible background | Current cube side 256; manual time/sky control. Biome attenuation is applied to HDR and sky consistently |
| Wind | Global weather plus Wind / Roads local ellipsoid volumes, deterministic periodic gusts | Combined speed capped at 5m/s; color and shadow share displacement |
| Shader graph | Graph: typed node connections and live transactional GLSL; imported fragment texture inputs and model/terrain/water/NPC targets | Texture sampling is fragment-only. Terrain/water vertex outputs rejected; target shaders retain their own optical/material models |
| Water | Water editor, ocean/pools/moat/pond shaders, authored closed-basin ponds and spline river channels/meshes | No hydrodynamic fluid solver, erosion coupling or interactive ripple solver |
| Kitbash appearance | Source masonry correction and consistent reviewed HDR/afternoon light; existing layouts preserved | This is an art-direction issue as well as tooling. Repetition, sparse street dressing and simple character silhouettes remain visible; AAA art completion is not claimed |
| Splines | Splines repeats real grounded/cullable assets; Wind / Roads grades roads and carves rivers | Bounded paths and placements; object spline bake creates editable instances |
| Optimization | Asset cooking/LOD, grouped draws, visibility/distance culling, worker decode/terrain generation, residency budgets, profiler and graphics settings | Full-route measurements, all frame misses and unchanged quality settings determine the result; average FPS alone is insufficient |
| Decal actors/projection boxes | Decals procedural soil/moss/wear/wet patches; Surfaces imported alpha/PBR boxes with XYZ rotation, feather and facing | Terrain and opaque scenery; nearest eight of each bounded projection family |

No new paid pack is needed to exercise these tools. More purchases do not repair missing material setup, uneven texel density, repeated placement, animation problems or profiling regressions. Additional assets should be selected only for a specific silhouette or scene gap after a reviewed art pass.
