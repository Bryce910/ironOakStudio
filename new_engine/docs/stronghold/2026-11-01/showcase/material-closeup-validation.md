# Source material close-up pass

This pass changes the source-private material shader only. Other projects and user-purchased source files are untouched. Production asset imports have not been changed by the candidate tools.

## Verified shader corrections

- Source ARM ambient occlusion now uses the authored channel directly rather than mapping it into 0.48–1. Unity’s source UnityGlobalIllumination.cginc applies occlusion to baked diffuse as well as indirect specular, so this is not an extra lightmap-darkening stage.
- Source roughness can reach 0.04 rather than being clamped to 0.25. Existing normal mip variance and screen derivative specular filtering remain active.
- Source snow cover uses the original 106m/8m height blend and the exact project origin. The old-layout rectangular indoor snow exclusion is removed. Untagged indoor materials are unaffected.

## Buttress lighting repair

134 native placements were conservatively excluded because two restored source mesh hashes can differ from per-instance painted UV1. Original prefab/fileID/world-transform recovery identifies 10 exact bindings; those sample original painted UV1. Other placements use only their verified same-instance LOD0 lightmap surface within 5cm and normal cosine 0.95. Restored mismatched UV1 is never sampled.

46,454 source vertices recover valid light; 1,466 uncertain vertices keep fallback. Across 13 GLBs this changes 50,846 GI vertex instances. All non-GI accessor storage remains byte-identical. New source anchors, bounds, transforms, LOD floor datums, indices and material metadata are unchanged.

The isolated catalog is `assets/purchased/nature_castle/showcase-buttress-gi/catalog.json`. `tools/integrate_nature_gi_candidates.py` stages new immutable imports by default. Its explicit `--apply` mode changes only matching asset-stem fields, preserving all other current document lines and source-keyed texture cache links. Eleven revisions are staged. Production apply is deferred until the route-capture lease ends.

## Selective texture candidate

`assets/purchased/nature_castle/showcase-closeup-pillars-final/catalog.json` upgrades only M_Pillar_stone_01 and M_Pillar_stone_02 albedo/normal to 2048px. Exact existing 512px ARM maps remain. The bake retains original sorted logical-unit first-coverage order. Native GI repair is included in this candidate; all geometry/GI accessor bytes and rendering metadata survive repacking unchanged.

42 files across 21 existing aliases change. The largest is 126.8306 MiB, below the 128 MiB loader cap. Three-material and higher ARM variants were rejected by this guard and their incomplete generated outputs removed. Source material caches are retained.

Opaque albedo JPEG96 with no chroma subsampling bounds package size; normals remain lossless PNG. Decoded JPEG mean error is 1.31–1.34/255, 99th-percentile channel error 4–5/255 and PSNR 43.3–43.5 dB. No alpha material uses JPEG. The uncompressed source candidate PNGs remain available for comparison.

Full-mip texture delta for the two pillars is 64 MiB with raw uploads, 16 MiB if all four images pass certified BC7, or 40 MiB with raw normals and compressed albedo. The first conservative CPU check exceeded the preparation budget; the renderer agent’s verified source-only decoder-scratch reuse fix resolves that accounting issue. The worst HighCastle and Temple chunks both pass the unchanged 768 MiB source preparation limit. Native RX5300 certification admits both 2K albedo caches; both 2K normals fail the unchanged angular guard and retain lossless uploads. Exact 512px ARM maps also retain originals. Actual cached texture delta relative to the existing pillar maps is +40 MiB. Only two new content-keyed cache files were added to the source project, and shared cache manifests were not altered. After certification, actual native CPU preparation retains 439.65 MiB for the worst HighCastle chunk (209.33 MiB texture uploads) and 342.27 MiB for Temple (169.33 MiB textures); both pass the existing 768 MiB limit. A final native close-up/performance comparison is still required before accepting the combined imports. No loader or memory cap has been raised.

## Character findings

The king head source and cooked albedo match (mean difference 0.54/255 at the cooked tier); missing/wrong base maps do not explain the dark crown stripes. The original FBX has Phong shininess 20 and beard transparency; it does not bind a PBR metallic/smoothness texture. The archive separately supplies Metallic/MOS packed maps, whose alpha interpretation therefore needs a verified source convention before inversion. The current cooker treats Metallic alpha as roughness with a floor; no speculative channel inversion was applied. Original GLB hair BLEND becomes MASK 0.35 in the current bounded pose cook, which can coarsen wispy hair. Character texture files and geometry remain unchanged in this pass.

## Evidence

- `pbr-candidate-math-audit.json`: finite source snow blend comparison.
- `buttress-uv1-bindings.json`: original prefab/fileID/transform binding evidence.
- `assets/purchased/nature_castle/showcase-buttress-gi/report.json`: conservative recovery and exact non-GI checks.
- `assets/purchased/nature_castle/showcase-buttress-integration/validation.json`: line-preservation/idempotence/cache-link checks.
- `assets/purchased/nature_castle/showcase-closeup-pillars-final/report.json`: all image-only geometry/metadata and size checks.
- `closeup-texture-quality.json`, `pillar-2k-exact-texture-budget.json`: candidate map measurements.
- `king-source-fbx-materials.log`: CPU-only original source material inspection.

The parent granted one exclusive native RX5300 session for offline texture-cache certification. That lease is released. No game/capture session was launched by this audit; the parent owns the final native A/B comparison.

## Combined staged revision plan

`assets/purchased/nature_castle/showcase-material-final-integration/plan.json` stages 21 immutable revisions combining the conservative buttress GI repair and the two selected pillar materials. Its preview changes only 21 asset-stem fields. All42 staged files match their verified candidate bytes, cache links resolve to the source-project cache, and near/unsuffixed files remain hard-linked within each revision. Apply only at the parent’s loading boundary with:

```sh
python3 new_engine/tools/integrate_nature_gi_candidates.py --output new_engine/assets/purchased/nature_castle/showcase-material-final-integration --apply
```

The explicit ancestor report allows either the original or the intermediate GI-only active import to be verified without rebuilding or overwriting the whole project. Production files remain independent of the staging inodes. The source-private `uActorTorchOnly` / `uActorLighting` guard has also been added in coordination with the publisher agent; it prevents new practical fixture light from duplicating the original static castle bake.
