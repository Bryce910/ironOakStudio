# Starting scenery loading and collision

The starting area uses the same native scenery/model renderer as the wider
world. `world_scenery_load_files()` merges the unchanged baseline file and the
new starting file into one transaction, sharing each alias/LOD group. The old
single-file API remains a wrapper. A failed file, model, bound, placement or
allocation frees the partial scene. Inputs are limited to eight files, 1,800
total records, 256 characters per line and 256 KiB per file. The fixed registry
has 26 families and at most 39 model groups; data cannot select arbitrary asset
paths.

Records retain the six existing columns, with an optional seventh elevation:

```
alias map_u map_v yaw_degrees scale burial_meters [elevation_meters]
```

Positive elevation up to 3 m is limited to the new crate, barrel, bucket,
lantern and axe aliases. It is applied after grounding. Same alias/root records
are rejected unless their elevations differ by at least 1 cm, allowing authored
stacks. Per-source stats report instances, trees, props, unique aliases and actual
collision shapes separately. Overall aliases remain unique across all sources.

Architecture and picnic tables use the terrain foundation height at their root.
Platforms have no horizontal collider; their top is terrain + 2 cm minus burial.
Their authored foundations must be flat. Rocks, deadwood and loose solid props
retain the lowest-support terrain scan and 5 cm contact slack. For a prop on a
table, add the table's scaled height plus that slack to its elevation, accounting
for burial. Crate stack increments use the lower crate's scaled height.

`world_scenery_can_move()` sweeps the player's horizontal disk continuously
through the whole requested displacement and checks feet/head overlap. The
viewer supplies current feet Y, body height and radius, then may retry X/Z axes
for sliding. It can query zero XZ displacement at a proposed feet height for
jump ceilings and landing contacts. Flying can deliberately bypass these checks.

Tree collision uses a physical trunk radius instead of its canopy bounds.
Rocks, walls and tables use rotated model footprints. Round towers, barrels,
stumps and fire pits use circular footprints. Flowers, ferns, small dressing
and flush deck surfaces do not block horizontal movement. Four cheap global
bounds reject distant shapes before exact circle or rounded rectangle-distance
checks. The query allocates nothing, reads no files/atlas/GL state and traverses
only the bounded loaded shape list. It catches tunneling even when both motion
endpoints are outside a wall. Initially overlapping motion may only retain or
reduce penetration, so landing/teleporting into a low prop does not trap the
player permanently.

The fort gate is three shapes rather than one solid bounding box. Measured
jambs leave a 3.50133 m clear opening; the conservative header begins 4.2 m above
the normalized base. A 0.30 m burial puts its 0.269356 m sill below ground. Its
piers are checked against the original trail walking capsule, while its opening
may legitimately straddle that trail. Flush planks receive the same trail
exception because they are walking surfaces. Other models retain the trail
footprint authoring check. The arch's upper curved void is conservatively closed
at its spring line; the normal player and jump arc fit below it.

Runtime selection stores each group's own placement indices, avoiding a scan
of every record for every group. Camera angle trigonometry is evaluated once
per update. Instance vectors are reserved at load time; unchanged batches do
not upload, and failed uploads retain the last published anchor.

CPU regressions cover the original 283-record scene, all 14 new families and
18 new model filenames, merged-source failures, source counts, elevation and
stacking, an accepted 1,800-record batch, zero runtime allocations, rotated and
corner capsule contacts, high-speed tunneling, physical tree trunks, actual
trail gate passage, blocked piers, head clearance and overlap escape. The real
model GL fixture loads both authored files, checks the editable JSON route with
three capsule lanes and half-meter sweeps, draws eight destinations with the
actual color/shadow shaders, and verifies complete shared texture teardown.

Collision remains an authored proxy system rather than triangle-mesh physics.
It does not simulate rigid bodies, leaning props or deforming branches. Floors
are authored to the terrain instead of defining arbitrary multistory navigation.
Native captures and frame timings are separate from these correctness checks.
