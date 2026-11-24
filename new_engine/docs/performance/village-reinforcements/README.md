# Imported toolkit and gate reinforcement validation

The append-only layer adds 30 hand tools from all 12 supplied source tool types,
two farm worktables, two benches, two cargo crates and two stationary armoured
horsemen. Four additional shield guards use eight genuine source idle poses
through the separate rooted watch runtime. They have no duplicate static
scenery records. Existing residents, training guards and pets are preserved.

The original 2,640 records and their serialized text remain an exact prefix of
the 2,678-record village. All 24 roads, old inspection cameras, walking routes,
collision boxes and demo-route include are preserved. Only eight additional
object ground-clear rectangles are appended to the terrain mask include. Four
virtual watch rectangles extend the actual all-pose footprint by 0.25 m and
clear wild grass around the fixed guards' boots; the entire preceding mask
remains an exact prefix, as checked by `watch-ground-tests.log`.
`preservation.json` records the checks and final instance hash.

Sixteen tools rest on actual sampled table/bench/crate surfaces, verified with
80 source triangle rays. Fourteen long tools retain floor contact and lean
toward actual fence rail faces with a 6 mm standoff. The armory hammer sits on
the recessed wood crate top; its measured support proof narrowly exempts it
from the earlier expansion validator's solid parent-AABB approximation. Its
source support, other-object clearance and canonical placement remain checked
by this layer and the independent acceptance checker.

Both horsemen face the city at yaw π; their rotated asymmetric source bounds
are used directly. The four fixed watch stations are at (−8.5,−48), (8.5,−48),
(−10.5,193) and (10.5,193), with phases 0, .27, .53 and .79. The all-eight-pose
union clears protected routes by at least 6.29 m. The two castle-front posts
may occupy the broad paved court cap; the crown avenue and every walking or
actor corridor remain clear. All new cameras have source-clear body bounds
and dry, flat terrain footing.

Inspection views:

- `castle-watch`: wide city-facing view of the mounted and shield gate watch.
- `castle-watch-close`: close shield armour and guarded-idle view.
- `castle-farm-tools`: close view of the corn-side worktable and hand tools.

Evidence includes `cook.log`, `unit-tests.log` (seven source/negative tests),
`native-contract-test.log`, `existing-layer-tests.log`, `acceptance.json`,
`ground-contacts.json`, and `ground-validation.log`. The ground history is
rebuilt from the current scene and includes all eight new actual furniture and
horse contact sections, plus four virtual worn watch posts measured from real
0.10 m boot-height sections of all eight idle poses. Source geometry tests do not establish visual quality
or native frame rate; native evidence is recorded below.

The source asset inventories are
`models/village_toolkit_manifest.json`, `models/village_horseman_manifest.json`
and `models/village_shield_knight_manifest.json` under the Valwood project.
The toolkit's original source and packed images, provenance, exact recook proof
and CPU preview remain under `assets/purchased/medieval_tools/`.


## Native visual and traversal verification

Open `Launch_Valwood_Gate_Watch.sh` from the project folder. WASD walks; hold
right mouse to look; Shift runs; H shows controls and Scenic trails. The normal
castle launcher also accepts `--stop watch`, `--stop watch-close` and
`--stop farm-tools`. Close the window to quit.

Final real-game captures at 1600×900 on the RX 5300 were visually inspected:
[gate watch](captures/watch-view.png), [shield guard and horseman](captures/watch-close-view.png),
[farm worktable](captures/farm-tools-view.png). Pre-footing captures remain in
`captures/before-watch-footing/` for comparison. The final worn post exposes the
boots without changing guard roots or placement. These are native game pixels;
CPU previews in [ASSET_IMPORTS.md](ASSET_IMPORTS.md) are documented separately.

The 176 m farm loop completes with zero blocked/slope/sliding steps, exact return,
and all residents, pets, practice actors and new watch guards running.
[Native route acceptance](farm-native-acceptance.json) passes its structural and
45 FPS timing gates. All six production profiles have zero GL errors, failed
tiles, travel-loading frames, unmanaged texture uploads or remaining texture
references. No population or graphics-quality settings were reduced.

## Performance and limits

All production runs use normal afternoon lighting, 1600×900, the 60 FPS cap,
3072 px shadows and 9 shadow filter taps. Timed runs are sequential and contain
no animation-capture readbacks. `native` contains the first four checks;
`native-repeat` retains the two unchanged repeats, including failures.
Both folders have identical binary/source/model/ground hash inventories.

| Run | View/route | Mean FPS | 1% low FPS | p99 ms | Worst ms | Frames within 45 FPS |
|---|---|---:|---:|---:|---:|---:|
| native | watch-close | 57.850 | 24.760 | 35.261 | 47.827 | 97.083% |
| native | farm-tools | 59.520 | 48.230 | 19.917 | 25.779 | 99.881% |
| native | hero-mid | 46.150 | 6.450 | 57.972 | 488.210 | 75.000% |
| native | farm-route | 59.830 | 53.770 | 18.084 | 20.503 | 100.000% |
| native-repeat | watch-close | 58.930 | 37.700 | 22.635 | 39.438 | 98.810% |
| native-repeat | hero-mid | 47.460 | 6.770 | 52.804 | 470.906 | 78.690% |

The farm workstations and walking loop pass the strict 45 FPS gate. The close
gate view and detailed market do not: repeats still contain stalls. This
integration does **not** establish reliable 45–60FPS throughout the district.
The market first averaged 46.15FPS and repeated at 47.46FPS, with individual
stalls up to 488.210ms. All original results are retained, not replaced by the
better repeat. The new watch CPU path is fixed storage, four actor roots,
eight shared pose batches and at most four small instance uploads per frame;
600 seconds of CPU checks show zero hot allocations, stable roots, correct
culling/reentry, continuous weights and pass parity.

[Initial trace audit](performance-audit.txt) found no supported watch-module bug.
The final engine-only [temporary A/B diagnosis](diagnostic-ab/results.json)
disabled watch loading without modifying production files. With current static
assets the market averaged 52.70FPS; with the exact previous 2,640-record castle
file and old ground image it averaged 57.00FPS. Both still stuttered (1% lows
11.14/23.60FPS). The temporary build retained the twelve new compiled ground
masks; it is a comparison scene rather than an exact old executable.

The pair also recorded changing aggregate memory/IO pressure; owned benchmark
processes had no swap use. These observations do not isolate a single cause.
Both production `native` runs retain driver VRAM/GTT samples; the diagnostic
pair and `repeat-system-pressure.json` record aggregate pressure and only owned
benchmark descendants. No private browser data was inspected. Further frame
pacing/residency investigation remains warranted before claiming a smooth
45 FPS floor. The final production build keeps all requested assets enabled.
