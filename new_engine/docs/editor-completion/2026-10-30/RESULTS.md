# Editor completion pass — 30 October 2026

Scope: the requested object-transform, saving/recovery and responsiveness/delivery work in Valwood's existing custom engine. Character retargeting and global NPC navigation were not part of these three items.

## Implemented

- Inspector position XYZ, yaw/pitch/roll and independent positive X/Y/Z scale factors. Static rendering, inverse-transpose normals, tangents, shadow/reflection vertex paths, LOD padding, picking, wire bounds, source overrides, duplicate/prefab persistence and conservative collision follow the shape. Studio schema 6 and entity schema 4 read their earlier versions. Nonrepresentable hierarchy shear and unsupported typed-attachment transforms reject before publication.
- A whole-project authoring checkpoint captures the applied scene plus current physics, gameplay, living definitions, component references and NPC memory/appearance assets. Background serialization and file publication use a bounded write-ahead journal, previous generation, explicit finish/rollback and external-edit guards. Recovery/restore requires restart to avoid mixed live subsystem generations. Scene-draft recovery is synchronized after project Save. Individual tool histories and player-progress saves remain separate.
- Close protection accounts for typed subsystem drafts; save-and-quit uses the project checkpoint. Existing scene-only shortcuts remain compatible. Project saves preserve purchased source assets.
- Object edits retain unchanged terrain data/workers and avoid the global 1025 × 577 terrain-height upload. Unchanged shelter masks, surfaces and graph programs are reused. Actual tree/building/bounds, terrain, water and mask changes invalidate the relevant data. The panel exposes Apply time and residency pressure.
- Fixed incremental build dependency ordering: production/editor modules were appended after dependency expansion, allowing stale shared structure layouts. Dependency files now load after module registration. A complete rebuild and native retest followed.
- Updated editor guide, dedicated editor launcher, schema-aware inventory, ELF/glibc requirements and a packaged compatibility preflight. Windows/macOS native exports remain unimplemented/unvalidated; metadata checks do not certify a GPU driver.

## Verification completed

The release executable is SHA256 `62a11e66f6cef55bfda3407607c2ec271bb358b4b55131ab278903f4a9edf642`.

CPU contracts cover full transform matrix/inverse/hierarchy/picking/scene and prefab round trips, 318 designer commands, existing prefab/source identity/recovery workflows, live components, parser/collision/allocation behavior, interrupted grouped publication/replay/rollback, concurrency guards, paths/symlinks and malformed/oversized data. Inventory and packaging suites cover the new schema and portability checks.

The hardware vertex test uses the production shader on the RX 5300: 110 full/legacy TRS cases plus a real 19,728-indexed-vertex crate through the optional asset instance buffer, legacy reset and invalid-input preservation. It finishes with GL0 and no texture references. The test suppresses only a redundant same-program bind during transform-feedback capture; production upload, material, VAO and draw code are exercised.

The disposable-project native workflow passed 20 checks: full source transforms, terrain/shelter retention, two background checkpoints, previous-generation restore, recovery-base synchronization, grouped physics/gameplay rollback and the restart fence. The latest prop Apply took 101.108 ms; earlier candidate runs were around 1.1 seconds. These are individual observations, not a controlled universal speedup claim. Source world/settings/protected AI files were unchanged.

The dedicated editor run passed 67 workflow and 137 optimization checks with all 25 panels rendered. Across 1,001 steady frames after frame 800, it measured 59.951 FPS average, 57.847 FPS 1% low, 17.439 ms worst frame and no frames below 45. Mean editor CPU time was 0.1774 ms. Explicit Apply/loading frames remain in the raw profile and are excluded from this steady result.

Evidence: `final-contracts.log`, `final-scenery-gl.log`, `real-asset-native-gl.log`, `native-release/results.json`, `editor-steady/results.json`, `dependency-regression.txt`, `asset-database-contracts.log`, `package-contracts-final.log` and `preflight-contracts.log`.

## Walking and delivery

At unchanged 1600 × 900 output / 1120 × 630 internal resolution (70%), automatic quality off, reflections/contact shadows/volumetric fog/clouds retained:

| View | Route | Average FPS | 1% low | Sub-45 frames |
| --- | --- | ---: | ---: | ---: |
| third | rainforest | 57.274 | 46.619 | 9 |
| third | castle-west-street | 59.766 | 50.322 | 2 |
| first | rainforest | 57.694 | 47.381 | 5 |
| first | castle-west-street | 59.841 | 51.689 | 0 |

All four full return walks passed correctness, zero collision/slope blocks, GL0/ref0, fixed-quality and unchanged-source gates. All passed the statistical target. Three runs had isolated budget misses; only first-person castle passed the strict no-miss floor. This is route-specific evidence, not an all-world guarantee. Raw frames and source freezes are retained in `routes-third/` and `routes-first/`.

The development package is `builds/relocated/Valwood Editor 2026-10-30`: 1,474 files / 6,600,614,823 bytes, content ID `571d35b224bf4eeccc50a70b0a4677ab590196e963c11f23827b8a4770669ddf`. Its original build path was absent during both native launches from `/tmp`. The editor repeated all 204 checks; editor and jungle runs ended GL0/ref0 with no pending or failed streaming groups. Full package hashes remained unchanged. The source workspace remained mounted; this does not certify a clean machine.

The compatibility preflight passed on this Linux/glibc 2.39 host; this bundle requires glibc 2.38 or newer and OpenGL 3.3. The relocated authoring wrapper validated all current project data with zero warnings. Import help, executable Memory Studio help and the packaged frame-audit helper passed. Twelve package contracts and preflight positive/negative cases passed.

Packaging candidates with the initial indirect-library false alarm and missing helper execution permission were rejected, corrected and rebuilt. Small manifests/logs are retained under `package-initial-candidate/` and `package-tooling-candidate/`; their redundant generated multi-gigabyte payloads were removed. Earlier delivered packages were untouched.


The full walks recorded 16 brief budget misses across 35,920 measured frames. Their raw data remain in the evidence directories.

## Boundaries

The full-transform viewport uses the inspector for pitch/roll and axis factors; the existing viewport handles remain move/yaw/uniform scale. Mirrored scale and shear are rejected. Static collision uses conservative enclosing boxes for tilted pieces; arbitrary mesh collision is not added. Live physics/NPC bindings and typed water/routes retain explicit adapter limits.

Whole-project checkpoints are explicit saves. They do not replace every tool's Undo history or autosave every unsaved subsystem gesture. Browser memory drafts must be exported before they become engine assets. After a project restore, restart is required. GPU shader compilation, cold imports and real terrain rebuilds can still pause. A small measured route set does not certify every location or arbitrary authored content at a strict 45 FPS floor.

Initial failed native attempts and their debugger traces are retained. The startup crash was traced to stale production-module structure layouts; it was not accepted as a pass. Corrected-build native tests subsequently passed. Failed test fixtures and earlier candidate results remain labeled by their directories/log names.
