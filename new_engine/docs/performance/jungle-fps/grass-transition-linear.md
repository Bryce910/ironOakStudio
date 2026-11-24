The component profiler isolated the publication spike. On the ideal rainforest
walk, frame 257 spent 2.41550 ms in Dressing: 0.30467 ms pumping 1,802 candidates,
1.57955 ms replacing grass, and 0.53101 ms selecting vegetation. Frame 172 spent
0.30936 ms in Dressing, almost entirely selection, with no pumping or
publication. The recorded CSV and summary are `dressing-update-components.csv`
and `dressing-update-components.json`. This CPU fixture excludes the native
driver and scenery collision queries; these measurements identify component
work rather than explain every native frame delay.

WorldGrassTransition now keeps its identity lookup sorted across successful
replacement and outgoing-root compaction. Replacement matches the sorted wanted
and old identity streams linearly, stores matches by desired source index, then
merges desired IDs with retained outgoing IDs into the next lookup. Desired
root order, outgoing opacity/identity ranking, cap behavior, fades, world-space
state and invalid-input handling remain unchanged. Preallocated index maps and
staging lookup storage avoid allocation in replacement and advancement.

The differential fixture compares against a frozen copy of the production
header from before this optimization. All 1,845 comparisons matched complete
instance/fade bytes, absolute world-coordinate state bits, desired flags,
activity and structural revision. It independently validates the sorted lookup
against every live root after each operation. Cases cover replacement,
reordering, reentry, arbitrary compaction, invalid inputs, teleports, reset,
empty fields and saturated overlapping generations. A positive allocation
control precedes zero-allocation guards. The existing transition regression
also passed.

Isolated timings use actual atlas/cover generations, alternating legacy and
optimized order for 40 rounds with four warmup rounds. Every timed operation is
followed by a complete byte/state comparison:

| Actual replacement | Legacy mean | Optimized mean | Reduction |
| --- | ---: | ---: | ---: |
| Rainforest generation 2, 8,454 → 8,602 roots | 1.537398 ms | 0.870128 ms | 0.667270 ms |
| Rainforest generation 3, 8,602 → 7,794 roots | 1.489445 ms | 0.836281 ms | 0.653164 ms |
| Castle, 46,000 → 46,000 roots | 10.780217 ms | 5.992008 ms | 4.788209 ms |

Maintaining the cache adds approximately 0.047 ms to rainforest compaction and
0.336 ms to the dense castle compaction. It avoids repeating the old full sort
and binary searches at the larger replacement spike. These are CPU measurements;
native frame-time acceptance remains separate. Complete logs, including the
initial fixture compiler warning and corrected passing runs, are retained in
`grass-transition-linear-cpu.log`.
