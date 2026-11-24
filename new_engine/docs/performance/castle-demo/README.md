# Castle district demo verification

Measured on the AMD Radeon RX 5300 at 1600×900, capped at 60 FPS. All six native runs passed the 45 FPS mean, 1% low and 99th-percentile targets. Source and runtime asset hashes remained unchanged throughout measurement; builds and asset cooking were paused.

| View / route | Mean FPS | 1% low FPS | p99 ms | Worst ms |
|---|---:|---:|---:|---:|
| housing | 59.95 | 55.91 | 16.767 | 27.820 |
| marketrow | 59.95 | 58.11 | 16.740 | 23.830 |
| stalls | 59.95 | 58.57 | 16.727 | 21.842 |
| forecourt | 59.95 | 59.54 | 16.724 | 17.326 |
| hall | 59.95 | 58.42 | 16.719 | 22.640 |
| demo-route | 59.95 | 58.34 | 16.733 | 23.622 |

The full 1,011.5 m walk completed and returned exactly to its start, with zero collision blocks, slope blocks or sliding steps. All 64 residents and smooth GPU pose interpolation remained enabled. The final scene has 2,008 castle placements and 4,619 combined placements, 55 homes and four connected streets.

There are occasional frame spikes: the worst measured frame was 27.82 ms. These measurements establish the tested demo's performance, not a guarantee for other resolutions or every part of the 100 km world.

The native shader tests pass for the four bounded hall lights and 2049-square ground field. Source-model, placement, support, paving, crowd and collision checks pass. Peak whole-system Radeon VRAM was 2,990,874,624 of 3,204,448,256 bytes, leaving approximately 203.7 MiB at the observed peak; this includes desktop and other applications.

See [verification.json](verification.json) for the complete acceptance record, [native/results.json](native/results.json) for measurements, and [native/source-freeze.json](native/source-freeze.json) for hashes. Native screenshots are in [final-captures](final-captures/).

This is a freely walkable visual demo. Quests, dialogue, trading, combat and interiors beyond the keep receiving hall remain outside its scope.
