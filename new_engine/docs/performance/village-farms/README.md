# Corn and wheat farm district

The northeast district sits inside the outer town walls and east of the inner
castle wall. The corn field is local X 109..136, Z−184..−140; wheat is
X 147..174 with the same Z range. Two 27 × 44 m plots total 2,376 m². Six new paths
connect a 107 m approach and 176 m closed inspection walk to the existing streets.
The moat stays outside the outer town walls.

Launch `Launch_Valwood_Farms_Demo.sh` from the project folder. It starts between
the fields in the editable afternoon lighting look. WASD walks, right mouse
looks, Shift runs, H exposes controls and Scenic trails, Tab opens the map,
and F switches walking/flying. Close the window to quit.

The same launcher accepts `--mode guided` for the farm walk or `--mode benchmark`
for the native timing gate. `--lighting dusk` selects the existing dusk look.

## Assets and rendering

181 new placements comprise 120 crop clusters, 44 wood rail fences, 12 ground farm
details and 5 supported cart/tool items. The 120 instanced clusters represent
1,200 corn stalks and 21,120 wheat stems. Corn and wheat each have near and mid meshes
with common roots and source layout; complementary stable 56–64 m masks and a
280–320 m distant fade avoid switching whole fields at once. Quadratic wind pins
the roots, preserves height and uses matching color/shadow poses and correct
normal/tangent bases. Near crops use their lighter mid shadow proxy; distant
crops do not add all-distance shadow passes.

Original procedural crop sources, deterministic GLB builds, 512 px shared PBR maps,
and provenance are in `assets/authored/village_crops` and
`tools/build_village_crop_assets.py`. Three shared RGBA8 images with mipmaps cost
about 4 MiB; all four GLBs total 1.82 MB. These are authored game assets, with no
new third-party downloads required. Corn near/mid counts are 5,080/1,480 triangles
per cluster; wheat 7,744/2,816.

Soil uses the existing terrain dirt texture and analytic row-aligned relief,
filtered away when subpixel. No new terrain samplers or height displacement.
The two cultivated masks suppress wild grass while preserving the exact floor,
road/paving, ecology and creek sampling outside the plots. Crop geometry is
non-solid so players can enter fields; fences and cargo retain the normal solid
scenery rules. Harvesting, crop growth and new farmer NPC routines are not part
of this visual farm addition.

## Verification

- [Scene acceptance](scene-acceptance.json) checks current model hashes and
  placement data. [Integration proof](integration-proof.json) records the exact
  2,459-record/text and 18-road prefixes preserved, unchanged protected scenery,
  2,640 castle / 5,251 combined placements, new asset budget and published ground hash.
- [Staged proof](stage.json) covers source mesh and wind bounds, old structure and
  protected route clearance, 3,400 terrain rays and 25 actual cargo/tool supports.
  Seven farm tests and 37 castle acceptance tests passed before the six additional
  isolated launch tests. Production CLI allowlist/default-route tests also pass.
- CPU actual-loader testing checks Root/None registration, four crop groups,
  correct near-only shadow proxies and 600 seconds with no stationary allocations
  or uploads. Atlas tests compare enabled/disabled farm masks at 2,733 probes,
  including 1,216 formerly wild interiors and 573 exterior/street controls.
- The real RX 5300 passes actual shader tests: 7,680 wind vertices with maximum
  displacement 0.288138 m, fixed roots/Y, finite-difference basis agreement,
  exact color/shadow and repeated poses, 128 m rebase invariance, complementary
  LOD/fade coverage; 1,740 terrain shader samples verify row phase, boundary
  continuity, filtered slopes and exact unchanged count-zero/exterior behavior.
  Software GL and shared foliage/shadow-filter regression checks also pass.
- [South view](captures/farms-view.png) and [outer-wall return view](captures/farms-reverse-view.png)
  are actual 1600×900 native game captures, visually inspected. They confirm both
  crop types, cultivated soil and the correct relationship to the two walls.

## Native performance

Final timings are recorded in `native/results.json`, with full per-frame data,
world reports, driver memory, logs and a frozen binary/source/asset inventory.
The visible native runs use 1600×900, the normal afternoon look, 60 FPS cap,
3072 px shadow maps, existing residents/trainees/pets and no capture-sequence
readbacks during timing. Only one GPU run is active at a time.

| Check | Mean FPS | 1% low FPS | p99 ms | Worst ms | Frames within 45 FPS |
|---|---:|---:|---:|---:|---:|
| farms | 59.700 | 38.940 | 16.823 | 141.530 | 99.821% |
| farms-reverse | 59.950 | 59.080 | 16.788 | 17.823 | 100.000% |
| farm-route | 59.800 | 44.670 | 16.843 | 154.646 | 99.875% |
| hero-mid | 59.950 | 45.790 | 16.945 | 34.455 | 99.583% |

The farm route completes all 176 m at 3.5 m/s, returns exactly to its start and has
zero blocked, slope-blocked or sliding steps. Every run has zero GL errors,
failed tiles, travel-loading frames and remaining texture/cache references.
The farm native acceptance is [recorded separately](farm-native-acceptance.json):
its structural route checks pass and its performance flag is correctly false.

The strict 45 FPS timing gate fails the south farm view and farm route because
brief hitches lower their 1% lows. The longest retained frame is 154.646 ms.
The south view includes a 141.530 ms frame dominated by 138.842 ms in present;
its CPU update/render/editor work totals about 2.7 ms. This records where the
wait was observed; it does not establish the underlying cause. Intermittent
stalls also exist in earlier castle/torch measurements. The new checks establish
roughly 60 FPS typical performance in these scenes, with 99.58–100% of samples
inside the 45 FPS frame budget. They do not establish a locked 45 FPS floor.
All four runs are retained; the profile command exits nonzero for timing failures.

Whole-system driver VRAM peaks at 2847.66 MiB of
3056 MiB (208.34 MiB remaining), including
the desktop/apps; GTT usage is also retained. The final source and runtime asset
hashes match all 424 files in the timed freeze. No quality settings or
population counts were reduced to obtain these measurements. These results
cover the two listed farm views, their 176 m loop and the existing hero-mid street,
at the tested resolution and lighting; other locations need their own checks.

