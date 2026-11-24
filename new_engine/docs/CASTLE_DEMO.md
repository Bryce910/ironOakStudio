# Valdoa castle district demo

Open **Launch_Valwood_Castle_Demo.sh** in the project folder to walk the district.
It starts beside the lowered castle drawbridge, at 1600×900 with overlays
hidden. Close another running Valwood window first; the launcher uses the
engine's existing single-session lock.

The district contains 55 homes across Merchants Lane, Market Row, Southgate Lane
and Coopers Return, connected to the market and castle approach. Its 2,678 scene
objects include 479 service/market details, 96 bunting pieces and 126 doorstep
objects, plus 142 market additions: eight distinct counters, two covered stalls,
two loaded handcarts, banners and 86 supported goods. The hero street adds 135
placements: two full-size cloth-roofed shops fitted between existing homes,
three suspended cloth lines, six tall merchant hooks and four layered street
workstations. Two source-measured paving aprons connect the shops to the street;
36 bounded grove placements frame the distant town edges. The combined scene
contains 5,289 scenery placements. The earlier hero-street additions reuse
existing models and textures.

The castle expansion adds a west training court, a roughly seven-metre Defender
statue in the east court, an armory terrace, barracks and quartermaster buildings,
and paved connections. Its 116 added placements comprise 63 training/statue/armory
objects, one drawbridge, 16 castle-wing placements and 36 creek-bank placements.
The new buildings use closed source meshes; their interiors are not walkable.

A creek with a carved terrain bed forms a moat outside the outer town walls. Its
water surface remains separate from the walking floor of the static, lowered
timber drawbridge at the southern town gate. The interior courts, castle foundations and existing streets remain
intact. Bank plants, stones and local reflections connect the water to the scene.
New castle assets reuse resident image and sampler policies; they add no new GPU
texture policies.

The purchased torch layer adds six standing poles, fourteen measured wall brackets,
and two court braziers. The models share source-derived PBR maps and glowing-fuel
emission; procedural flames and sparks use the same smooth flicker as nearby
warm lighting on stone, terrain, people and water. At most eight local lights
run together. Wall planes and the receiving-hall envelope limit spill; the torch
lights use these masks rather than point-light shadow maps.

Open **Launch_Valwood_Torch_Demo.sh** for the detailed market street at dusk.
The ordinary castle launcher retains its afternoon look. You can also run
`Launch_Valwood_Castle_Demo.sh --lighting dusk --stop drawbridge` or select a
stop such as `training`, `statue`, `hero`, or `hall`. H exposes the same editable
lighting controls in either preset.

The northeast farm district sits inside the outer town wall and east of the
inner castle wall. Two 27 × 44 m plots contain corn and wheat on 2,376 m² of
cultivated soil, with rooted wind, row-aligned furrows, fences, seed sacks,
barrels, baskets, a loaded cart and farm tools. Six connected paths retain open
headlands and aisles. Its 120 instanced crop clusters represent 1,200 corn stalks
and 21,120 wheat stems. The existing castle, street and population placements
remain unchanged.

Open **Launch_Valwood_Farms_Demo.sh** to start between the fields. The ordinary
castle launcher also accepts `--stop farms` and `--stop farms-reverse`; H then
**Scenic trails** selects the same views. Use `Launch_Valwood_Farms_Demo.sh --mode guided` for the 176 m closed farm
walk, or `--mode benchmark` to require its timing gate. The native helper also
accepts `--scene farm-route`. These are visual crops;
harvesting and growth simulation are not implemented.

The supplied reinforcement packs add 30 hand tools from twelve types, six
supporting furniture props and two stationary armored horsemen at the town gate.
Four additional shield guards occupy the town gate and inner castle approach.
Their eight genuine guarded-idle samples interpolate on the GPU, with fixed
roots and shared color/shadow poses. Small worn watch-post patches keep grass
clear of their boots. These guards supplement the existing 128 residents and
seven training actors. Horseback movement is not implemented in this addition.

Open **Launch_Valwood_Gate_Watch.sh** for the new gate watch, or use the castle
launcher with `--stop watch`, `--stop watch-close` or `--stop farm-tools`. H then
**Scenic trails** offers the same inspection stops. The tools rest on measured
work surfaces or against actual fence rails; they are scenery props.
[Import and runtime verification](performance/village-reinforcements/README.md)
contains source details, captures and measured performance.

Castle inspection views use the selected **C lighting look** and 3072-pixel
shadow maps. Their settings remain fixed during the final performance checks.

There are 128 townspeople: 16 knights, 16 archers, 20 traders and 76 travelers.
Seven additional training actors occupy the west court: six poleaxe pupils
practise in pairs or at dummies, while a master of arms supervises. Their genuine
source attack motion and authored recovery blend between 24 poses on the GPU;
the instructor uses the existing breathing poses. These are visual drills.
Source attribution and adaptations are in
[CREDITS_CASTLE_TRAINING.txt](../projects/valwood_world/CREDITS_CASTLE_TRAINING.txt).
The original 64 residents retain their appearances, routes and animation phases;
new walkers fill gaps in those routes and new merchants staff the added displays.
Six cats and six Labradors idle or patrol around the market and village approach.
Animal source credits and conversion details are in
[PROVENANCE.json](../assets/purchased/pets/PROVENANCE.json).
The keep's walkable receiving hall has 40 furnishings, included in the 479 details,
with a clear central aisle through the entrance.

- WASD: walk. Shift: run.
- Hold the right mouse button: look around. Esc: release the mouse.
- H: show controls/settings; **Scenic trails** selects an inspection stop.
- Tab: map. F: switch between walking and flying.
- Close the window to quit.

The 848.94 m castle return route crosses the exterior drawbridge, passes through
the town and visits the training court, Defender monument and armory approach. It uses 14,854
frames including warmup and capture padding. The 88 m hero return route walks
the detailed market street in both directions.
The closed 1,011.5 m district route visits the south gate, housing streets,
Merchants Lane, market, castle forecourt and receiving hall. Inspect the paving at grazing
angles, facade detail near the camera, overhead cloth, shaded eaves, distant
roof edges, and NPC walking as you move between streets. At the castle, inspect
the trainees and their shadows, the monument materials, creek banks, moving
water, reflections and the bridge crossing.

Start at a particular stop from a terminal:

```bash
./Launch_Valwood_Castle_Demo.sh --stop drawbridge
./Launch_Valwood_Castle_Demo.sh --stop creek
./Launch_Valwood_Castle_Demo.sh --stop training
./Launch_Valwood_Castle_Demo.sh --stop statue
./Launch_Valwood_Castle_Demo.sh --stop armory
./Launch_Valwood_Castle_Demo.sh --stop hero
./Launch_Valwood_Castle_Demo.sh --stop hero-mid
./Launch_Valwood_Castle_Demo.sh --stop hero-reverse
./Launch_Valwood_Castle_Demo.sh --stop alley
./Launch_Valwood_Castle_Demo.sh --stop arrival
./Launch_Valwood_Castle_Demo.sh --stop market
./Launch_Valwood_Castle_Demo.sh --stop stalls
./Launch_Valwood_Castle_Demo.sh --stop marketrow
./Launch_Valwood_Castle_Demo.sh --stop housing
./Launch_Valwood_Castle_Demo.sh --stop forecourt
./Launch_Valwood_Castle_Demo.sh --stop hall
```

Watch the castle additions, the short hero street walk or the complete district
route:

```bash
./Launch_Valwood_Castle_Demo.sh --mode guided --route castle
./Launch_Valwood_Castle_Demo.sh --mode guided --route hero
./Launch_Valwood_Castle_Demo.sh --mode guided --route district
```

Guided mode controls the camera and exits at the end. Close its window to stop
early. Movement advances at a fixed 60 Hz simulation rate and 3.5 m/s; slower
rendering can make the real viewing duration longer. Launch normally afterward
to explore yourself. The command prints its route length and report folder.

Run a native 1600×900 acceptance benchmark:

```bash
./Launch_Valwood_Castle_Demo.sh --mode benchmark --route castle --output /tmp/valdoa-castle-check
./Launch_Valwood_Castle_Demo.sh --mode benchmark --route hero --output /tmp/valdoa-hero-check
./Launch_Valwood_Castle_Demo.sh --mode benchmark --output /tmp/valdoa-demo-check
```

The benchmark checks the selected route, runtime source counts, 128 townspeople,
seven training actors and 12 pets, smooth GPU pose interpolation, zero
collision/slope/sliding interruptions,
resource cleanup and integrated lighting. Its 45 FPS target requires mean and
1% low frame rates of at least 45 FPS plus a 99th-percentile frame time no greater
than 22.22 ms. Each run reports its measured result. A failed target exits with
an error and preserves the original
timing/log files. Guided mode reports the same measurements without requiring
that timing target.

Check the authored data without opening a GPU window:

```bash
python3 new_engine/tools/check_castle_demo.py --hash-models
./Launch_Valwood_Castle_Demo.sh --dry-run
```

CPU acceptance checks committed placement bytes, protected scenery, actual
model hashes when requested, bounded instance counts, continuous route
clearance and paved-road coverage. It cannot establish frame rate or visual
quality. This is a visual and traversal demo with animated training drills.
Player combat, quests, dialogue, trading and interiors beyond the receiving hall
are outside its current scope.

## Earlier castle expansion performance

Before the torch and farm additions, eight native checks passed at 1600×900 on the RX 5300. Mean frame rates
are 59.81–59.95 FPS; 1% lows are 49.45–59.78 FPS. The 849 m castle return completes
at 59.92 FPS with a 55.73 FPS 1% low, without blocked or sliding steps. The retained
1,011.5 m district route also completes and meets the 45 FPS threshold, but has a
brief hitch burst including a 145.636 ms frame. The castle return's longest frame
is 21.226 ms. These timings include 128 townspeople, seven training actors, 12 pets,
local reflections and 3072-pixel shadow maps.

[Final castle verification](performance/castle-expansion/README.md) contains all
view/route metrics, unchanged-source checks, native captures and raw evidence.
The hidden-HUD path consumes queued input while avoiding desktop mouse queries;
visible controls retain the normal editor path.

[The previous market verification](performance/market-hero/README.md) remains
historical. These checks cover the listed views and routes at the tested
resolution; they do not certify performance everywhere in the world.

[Current farm verification](performance/village-farms/README.md) contains the latest
farm views, traversal results, captured images and performance limits.
