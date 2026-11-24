# Expanded purchased village and stable rendering

The custom engine now uses all six purchased house families across 42 homes, four shop families across 8 booths, 163 detailed wall panels, 2 closed side gates, 30 barrels, 48 crates and 194 market goods. All five purchased character families render 29 of the 64 residents. The complete original keep remains active: the supplied castle composite has disconnected elevated masses and is excluded from the scene. Originals and conversion/repair proofs are retained.

## Rendering and geometry

Canonical depth now matches unjittered color reconstruction on sloped surfaces. Leaf/sky coverage retains depth-supported history, with opaque/cloud silhouettes excluded. History confidence follows projected wind motion rather than a constant species value. End-to-end wall joins remove near-coplanar overlaps, and each House D LOD drops six redundant plaster faces while retaining the outward face and exact remaining UVs/images. Crowd lanes and phases keep all residents at least 1.4 m apart in 600 seconds of pairwise checks.

Matched native 1600×900 captures use the exact final executable, frozen wind/cloud/resident time, and active Halton jitter. Full-frame temporal luma variance falls 86.24% in the grove and 44.51% in the castle market. This isolates rendering instability; wind remains active during ordinary play and performance measurements. Fine distant foliage still has some aliasing. [Raw comparison](frozen/comparison.json).

## Native performance

AMD Radeon RX 5300, 1600×900, 60 FPS cap, first 120 frames excluded; no visual-QA readbacks in profiles.

| Scene | Mean FPS | 1% low FPS | Within 45 FPS budget |
|---|---:|---:|---:|
| market | 59.95 | 59.76 | 100.000% |
| village route | 59.95 | 59.71 | 100.000% |
| grove | 59.67 | 50.19 | 100.000% |
| rainforest | 53.23 | 45.19 | 99.702% |

The final 806 m village walk completes at 3.5 m/s with zero blocked, sliding, loading or grass-cap-drop frames and zero return error. The rainforest has 5 slight below 45 FPS frames among 1680 samples; its slowest frame is 22.474 ms.

## Verification and limits

The full test suite, focused final scene tests and 12 native renderer fixtures pass. Final source/assets/executable fingerprints remain unchanged across the profiles and all four stability captures. Runtime texture owners and GL errors return to zero in every profile. [Machine-readable report](results.json), [build inventory](final-build-manifest.json), [native checks](native-fixtures.log).

Houses use closed exteriors; canopy collision is conservative. Residents use scripted routes and sampled baked poses, with no skeletal interpolation or autonomous dialogue. The purchased castle source is retained inactive for future assembly review; every source/cooked vertex matches within 3.86 micrometres, confirming the source itself contains the detached masses.

[Final courtyard capture](captures/courtyard-view.png) · [Final market capture](frozen/after/castle.png)

The updated native preview was confirmed running at the market after verification.
