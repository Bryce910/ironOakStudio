The horizon broad phase indexes the published source field into 512 m cells. Each
cell keeps actual source X/Z and the existing float crown-height extrema. A
selection can reject a cell only when conservative distance, maximum-depth or
minimum-across supports prove that every root fails the existing point
predicate. The three original depth terms, including the pool mirror union,
remain represented. Bounds expand outward by 1 mm and final arithmetic receives
additional outward error guards. Invalid or unindexable input uses the original
full scan.

The retained roots still enter the unchanged point predicate in original source
order. Distance-pair arithmetic, nearest-root partitioning, tie behavior, bucket
caps and committed-buffer reuse remain unchanged. Index storage is allocated
once for 100000 source IDs and at most 4096 cells. Publication and selection do
not allocate.

`python3 tools/test_world_horizon_grid.py` compiles the actual HorizonForest
source plus an otherwise identical full-scan control against inert model
stubs and the real production atlas. The independent point reference covers
100000 synthetic roots and 1008 combinations of yaw, pitch, height, aspect and
mirror state, including distance/cell boundaries and equal-distance ties.
Uncapped pair order and capped results match exactly. Failed, replaced and
empty publications fail open correctly; a positive allocation control verifies
the guard before zero-allocation checks. The actual generated rainforest field
contains 95498 roots; 64 camera views produce identical selected pair order,
uploaded instance bytes and draw counts.

The isolated `--benchmark` run measured 1.100674 ms per full-scan selection versus
0.586684 ms with the grid across 64 views repeated 10 times in alternating order
(1.876x). This measures the CPU selector with inert GL stubs. Native frame-time
improvement remains subject to renderer acceptance. The full log, including
initial fixture compile/setup failures and corrected passing runs, is retained
in `horizon-grid-cpu.log`.
