# Expanded Emerald jungle

The authored exploration footprint grows from **210 × 235m (49,350m²)** to **210 × 470m (98,700m²)**. The existing trail joins a southern return loop and a short path to a hidden pool. A ruined shrine, canopy viewing clearing, boar glade and leopard glade provide destinations.

The isolated visual project adds **960 editable scenery objects and foliage islands**, including 220 canopy trees, 48 palms, 617 fern/broad-leaf/thicket islands, fallen wood, rocks, roots and trail markers. **862 new placements are more than 10m from a walking path**. Dense perimeter undergrowth helps the finished area read as forest when looking away from the trail. Individual islands contain multiple source plants; these counts are scene placements, not individual leaves/plants.

The active rainforest wildlife bank has six ground animals (four leopards and two wild boars) and six flying macaws. Ground animals wander/rest on feathered, level glades. Macaws flap and glide above the canopy, with continuous interpolation between 12 compatible poses. Wildlife remains ambient. The map in jungle review shows the connected route, destinations, player position and facing direction.

## Sources and boundaries

No assets were purchased or downloaded for this work. Vegetation derivatives reuse the project's existing owned/CC0 foliage, with near/mid geometry and shared source textures. Ground, wood and stone use existing project materials. The shrine and macaw geometry are authored in the deterministic recipe. The shrine uses 1K color and 512px normal/material maps.

`tools/build_jungle_expansion.py` rebuilds the private assets, layout manifest and canonical Studio document. It starts from the saved pre-expansion Studio document in the private asset folder. Re-running it intentionally replaces this authored expansion; preserve subsequent manual edits before rebuilding. `tools/author_jungle_expansion.cpp` uses the editor's terrain brushes, surface validation and atomic serializer. Source scenery files are unchanged.

The editor's added-model allowance is now 1024. Prepared scenery supports 8192 original source slots plus this allowance; ordinary source loading retains its 8192-record limit and 4096-per-alias limit. Original source object identities stay fixed. Roads cache vegetation suppression on their terrain grids, including shared tile edges, so generating cover does not scan spline segments per plant.

The running source project and jungle launcher use this expansion. Existing standalone presentation packages are not rebuilt by this change.

## Validation

Final native results are saved in `validated-views`, `validated-walk`, `validated-controller` and `validated-journey`. The final binary, Studio document, manifest, graphics preferences and private GLBs are fingerprinted in `final-inputs.json`. Native tests use isolated progress files; protected source scenery and player saves are fingerprinted separately.

CPU checks cover wildlife bank ownership, animated flight, no allocations/model IO during updates, Studio persistence and seams, controller routing, presentation progression/save/retry, editor copy/paste/terrain snapping, prepared scenery capacity and cached road masks. Native walking uses the actual movement/collision/terrain code at 3.5m/s, with automatic quality disabled and fixed 0.7 render scale. Visual inspections use full 1600 ×900 resolution. The birds inspection camera is above the canopy; the other destination views are grounded. Performance statistics from still views are not treated as a sustained walking benchmark.

One older shared-world target, `make test-world-wildlife-habitats`, retains a historical scenery fingerprint and its `--validate-only` phase reports “Protected scenery sources changed.” This task does not refresh that stored proof. The current catalog unit checks pass, and this expansion has separate native grounding/route checks and before/after source/save fingerprints.

## Final native measurements

- All six final views passed on the AMD Radeon RX 5300. Wildlife: 12 actors, three species, six flying birds, zero unsafe roots, zero load failures.
- The complete 1,031.04448m walk visited all 27 waypoints, used 17,702 actual movement commands and returned exactly to its start. No collisions, slope blocks or sliding steps occurred. Animals had zero unsafe roots across the run.
- Native controller QA passed 41 assertions, including reconnect and rumble routing. The complete quest rehearsal passed, including real defeat/retry, rewards, save/resume, ending and combat VFX checks.
- Every native run ended with GL error 0 and zero remaining texture references. Final frozen inputs and protected player saves/source scenery were unchanged.
- At 1600 ×900 display, fixed 70% render scale, 2048px shadows and reflections/contact shadows/fog/clouds enabled, the full walking run averaged 54.89 FPS; 1% low 37.35 FPS; worst frame 85.40ms. Both the statistical 45 FPS target and a strict 45 FPS floor remain unmet. These are reported separately from successful route/render correctness. Automatic quality was disabled for this measurement; the player's existing automatic-quality preferences were preserved.

See `validation-summary.json` and the per-run profiles for the exact final binary fingerprint and detailed evidence.
