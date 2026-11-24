# Independent Windows playtest opening review — 2 November 2026

**No opening blocker found in the three actual captures.** Reviewed `city.png`, `jungle.png`, `grandstone.png`, all three completed logs, profile/world JSON, command records and `results.json`. No game/GPU was launched and no frozen source, runtime or project was changed for this review.

These captures use the Windows executable **under Wine on the host AMD RX 5300**, at 1600×900 with fixed 70% scene scale. Runtime SHA256: `24600deb1683fe5364ac0bbc919977db4303a5b08de6a39f5256bb75917a0a87`. All three exited0; the runner records unchanged protected source state and binary. This is opening/rendered-HUD acceptance, not a sustained FPS or completed-walk result.

## Actual image review

- **City:** market geometry, paving, stalls and props are textured. No obvious missing-white-material fallback, large geometry hole or glossy-ground regression is visible. The title, `0 / 5 landmarks | Market stalls`, route minimap, approximately10m world marker and bottom controls are readable. First-person camera eyeY25.728487 matches the current tested support+1.75m; `fly=false`.
- **Jungle:** the worn path and dense foliage are visible and textured. The title, `0 / 5 landmarks | Hidden Pool bank`, minimap and39m next-point marker are legible. Camera eyeY137.072144 matches the recorded route support+1.75m; `fly=false`. Existing source-art/coarse-silhouette limitations remain, but no new material loss or large geometry gap is apparent.
- **Grandstone:** source stone pillar, stairs, statue and practical flame render. The title, `1 / 3 landmarks | Walk the marked floor`, minimap and6m marker are readable. Camera eyeY1215.701538 matches the approach's source-supported spawn feet1213.951515+1.75m to within0.00003m; `fly=false`. Its first checkpoint is the spawn and is already consumed by design. The still image does not establish that the stair route or later hall has been walked.

All three show player-scale first-person views at the expected starting heights, rather than elevated fly-through cameras. The images do not independently prove body contact everywhere; that requires the separately scheduled movement/collision checks.

## Diagnostics beyond the simple pass gate

All three world sidecars parse and report **zero** failed terrain tiles, streaming failures/critical pending assets, material-preparation failures, texture-cache rejections/upload failures, remaining decoded texture references, GL errors, particle failures, physics errors/heap fallbacks and gameplay failed ticks. Main material preparation is ready. No unexpected model/scenery/material/shader/collision-loading error appears in the logs.

| Area | Route at opening | Resident / wanted | Loading / failed / critical pending | Relevant actors |
|---|---|---:|---:|---|
| City | Node1/213;0/5 milestones |185/185|0/0/0|Source city logs53 authored static collision bodies.|
| Jungle | Node1/28;0/5 milestones |175/175|0/0/0|12 wildlife actors,3 species,6 flying birds; unsafe roots/load failures0.|
| Grandstone | Node1/3;1/3 milestones |127/150|1/0/0|21 residents,48 pose assets,11 visible; failures0.|

`complete=false`, zero walking frames and disabled input-QA are expected for held opening captures. They must not be relabeled as route/input passes. MID fallback counts3/62/100 and shadow-culling rejection counts are normal rendering decisions, not rejected asset imports. Grandstone retains one optional HIGH refinement request; this smoke does not establish full HIGH residency.

Each log contains38 missing **legacy optional** sound/music notices. The current demo audio bank is ready with zero update failures, but the driver is `dummy`, so these runs do not certify audible playback. Texture-cache misses select the existing decoded texture path; no cache rejection/upload failure occurred.

## Remaining acceptance

Proceed to the separately scheduled actual walking, section-transition, save/load and controls checks. Preserve the distinction between control-only synthetic section completion and genuinely walking Grandstone's supported sectors. These opening results do not certify a45FPS floor, all scene navigation, an untested Windows driver, physical controller behavior or final art quality. The city quest/vendor relocation and ambient-only jungle wildlife limits remain as documented in the route guides.
