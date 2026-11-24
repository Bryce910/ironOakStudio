The cached linear matcher still spent time sorting incoming identities and
outgoing opacity ranks. The incoming sort now uses eight stable 8-bit radix
passes for 64-bit identities. Outgoing roots start in cached identity order;
equal opacities need no further sorting. Mixed positive opacities use four
stable passes on complemented IEEE754 float bits, preserving the exact
opacity-descending, identity-ascending comparator. Small groups retain the
original comparator. Even pass counts return vectors to their original backing
stores, and the existing preallocated staging lookup provides scratch storage.

Before application, a temporary include overlay passed the 1,845-case frozen
legacy differential and the focused radix edge fixture. The latter covers
unsigned extremes, duplicate-key stability, positive float subnormals and
boundaries, opacity/identity ties, 12 sizes from zero through 92,000 records,
unchanged backing pointers, and zero guarded allocations. The edge fixture is
now permanent at `tests/world_grass_transition_radix_edges_test.cpp` and runs
with `tests/run_world_grass_transition_differential.py`; no temporary files are
required for production validation.

The isolated actual-cover benchmark used 40 alternating legacy/optimized rounds
with four warmup rounds and exact byte/state comparison after every operation:

| Actual replacement | Original legacy mean | Radix/cache mean |
| --- | ---: | ---: |
| Rainforest generation 2, 8,454 → 8,602 roots | 1.557646 ms | 0.478119 ms |
| Rainforest generation 3, 8,602 → 7,794 roots | 1.495502 ms | 0.454318 ms |
| Castle, 46,000 → 46,000 roots | 10.820349 ms | 2.941627 ms |

The earlier cached-linear implementation measured approximately 0.87/0.84 ms
for these rainforest replacements and 5.99 ms for the castle replacement.
Maintaining its sorted cache adds approximately 0.045 ms to rainforest
compaction and 0.324 ms to dense castle compaction compared with the original
legacy implementation. Radix sorting retains that existing compaction cost.

Root instances, output order, opacity values, 0.8-second transitions, revision
behavior, generation scheduling and image quality remain unchanged. These
measurements describe CPU replacement work; native frame-time acceptance is
separate. Complete overlay and installed-header validation is recorded in
`grass-transition-radix-cpu.log`.
