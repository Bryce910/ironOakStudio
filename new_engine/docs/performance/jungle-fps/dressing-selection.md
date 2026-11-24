The grass transition now exposes a structural revision. Successful replacement,
reset and removal of completed outgoing roots increment it. Opacity-only
advancement leaves it unchanged. GrassGPU uses this revision and root count to
cache its existing seeded classification. Current opacity, culling, instance
payloads and uploads continue to update as before.

Tree candidates now enter their existing species and LOD groups in one source
pass, calculating horizontal distance once per root. Photographed grass uses a
shared radial envelope and seeded-variant dispatch, then applies the same group
distance, sphere, wall and point predicates. Group candidates keep their
original source order. Caps, nearest-distance partitioning, sorting, transforms,
fades, shadow proxies and reflection lists remain unchanged. Unsupported or
duplicate group layouts use the existing per-group selection path.

`python3 tools/test_world_dressing_selection.py` extracts actual GrassGPU and
Dressing source into a CPU fixture. A control restores the old classification
condition and forces the original per-group scan. Additional controls isolate
the revision and dispatch changes. The real atlas and cover generator provide
rainforest and castle fields, including 46,000 castle grass roots and 7,000
trees per region. Synthetic endpoints add exact range boundaries, adjacent
float values and equal-distance ties.

All 284 cases matched every pre-cap distance/index pair in source order,
post-cap pair order, instance and opacity bytes, overflow payloads, shadow
proxies, reflection payloads and upload counts. Cases cover opacity progress,
compaction, replacement, reordering, failed replacement and upload, reset,
unusual group fallback, and loading-only probe restoration. A controlled
allocation verified the guard; selection had zero guarded allocations. The
classification cache performed 11 refreshes rather than 284, examining 309,179
seeds rather than 8,787,049.

The isolated CPU benchmark used a real rainforest transition snapshot with
10,345 grass roots, 7,000 trees and production group caps. Across 64 views
repeated 12 times in alternating order, the per-selection means were:

| Control | Milliseconds |
| --- | ---: |
| Original classification and per-group scan | 0.681717 |
| Structural revision only | 0.575070 |
| Single-pass dispatch only | 0.445376 |
| Both changes | 0.346932 |

The fixture uses inert GL/model stubs and disables candidate observation while
timing. These results measure CPU selection work; native image parity and
frame-time acceptance remain separate requirements. The log retains the
initial fixture compiler error and subsequent passing runs in
`dressing-selection-cpu.log`.
