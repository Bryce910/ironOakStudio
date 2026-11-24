# Surface completion acceptance checks

Surface contracts and the combined native editor workflow now have evidence. Final combined walking routes and packaging remain separate delivery gates.

| Contract | State | Evidence / next check |
| --- | --- | --- |
| Surface data codec preserves old state on malformed input | CPU passed | world_surface_authoring_test.cpp |
| Texture slots reject the wrong color-space role | CPU passed | Color image in Normal slot rejected |
| Terrain strokes compose/erase without weight overflow | CPU passed | Ordered two-layer composition plus erase |
| Shared tile edges remain identical | CPU passed | Two adjacent 65-sample tiles compared byte-for-byte |
| Brush coverage exceeding the tile budget preserves prior field | CPU passed | 65 distributed tiles reject |
| Projection orientation rejects the back face | CPU passed | Floor and rotated wall cases |
| Local wind is periodic/bounded and absent at volume edge | CPU passed | Deterministic time-wrap/edge/overlap cases |
| Splines retain height/distance and reject collapsed/invalid paths | CPU passed | Straight and smooth profiles, invalid layer/point cases |
| Packed surface textures have no quadrant/wrap seam | CPU passed | CPU equivalent sampler at both sides of boundaries and several mips |
| Color mips average in linear light | CPU passed | Black/white reduction approaches 0.5 linear, not 0.5 encoded |
| Packed mip rectangles and gutters do not overlap | CPU passed | Whole fixed mip-tail layout |
| Castle source masonry restores authored UVs | Stage passed | All 7,411 masonry triangles matched exactly across castle/gate/wall |
| Castle positions, triangle count, bounds stay exact | Stage passed | Whole before/after triangle-key comparison and manifest checks |
| Purchased originals and all middle LODs remain unchanged | Passed, published near maps only | Exact source/geometry proof and before backup in assets/purchased/medieval_village/castle-source-materials; active keep is village_keep |
| Surface apply/link succeeds within hardware sampler budget | Native passed | Full production shader fixture: 15 active model samplers of 16; combined surface-workflow1 phase7 GL0/ref0 |
| Failed image import or GLSL compile retains live world | Native passed | Combined workflow: three expected rejections, old live resources retained |
| Studio undo/save/reload persists each surface kind | Native passed | Combined surface-workflow1 restores prior state and reloads schema5 / surface2 |
| Terrain/model image projection has visible alpha/orientation/PBR | Native passed | Actual alpha/backface/terrain hash pixel checks and combined surface workflow |
| Graph texture samples and terrain/water/character targets execute | CPU/native passed, bounded scope | Texture graph compile/pixels, stage rejection, combined workflow; terrain/water vertex offsets unsupported |
| Local wind color/shadow and particles share field | CPU/native integration passed | Bounded periodic field tests; production color/shadow variants compile; same root used for reactivity. Separate motion-quality visual review remains |
| Road/river editing changes actual terrain and water/navigation | CPU/native passed | TerrainField tests plus combined Apply road height, river water query, mesh and rejected unsafe grade |
| Castle/tavern/jungle look | Native captures passed | Four town and two jungle first-person captures; GL0/ref0, same saved quality; look publication backup and protected-row hash proof |
| Existing castle/tavern/jungle walking at final look | Statistical target passed; strict floor failed | Full return walks: jungle58.039avg/45.049one%-low/25misses; castle59.887avg/49.732one%-low/2misses. All misses retained; Firefox closed for final test |
| Final package contains runtime dependencies and opens from another path | Passed | 1441 files / 6.48GB; relocated launch from /tmp with original package path absent, GL0/ref0; production live-world/native/package-final/result.json |

Reference GPU: native RX 5300, 3 GB. A route's average FPS does not prove a universal 45 FPS minimum. Preserve failed and incomplete profiles. Explicit editor import/Apply/loading stalls are tracked separately from ordinary walking.

Combined workflow evidence: ../../production/2026-10-29/live-world/native/surface-workflow1/results.json. The original workflow failure and timeout retry under this folder remain retained and do not count as passes. See USER-GUIDE.md for controls and explicit implementation limits.
