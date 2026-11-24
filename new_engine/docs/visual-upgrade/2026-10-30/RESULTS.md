# Valwood visual upgrade — 2026-10-30

The rendering, material and environment pass is packaged and ready for review. Character replacement and facial animation remain unfinished. This build retains the existing hero and NPC geometry.

## Implemented

- Recovered the owned knight’s original texture set at 2K base color, with 1K normal/material maps. All 216 body/arms poses retain identical vertex, index and animation bytes.
- Removed unintended emission and default metallic skin from the live conversation NPC. Existing head/body animation remains.
- Added 16 window assemblies to eight selected houses and four slits to the court wall. Timber frames, hinged shutters, sills, recessed backs and reflective opaque panes add depth. The source walls have no cut-through holes or enterable interiors.
- Added photographed forest litter with height-guided blending, OpenGL normals, AO and roughness. Paths retain their existing surface and terrain geometry.
- Authored a jungle pocket with 87 ten-clump grass patches, 45 understory plants, eight root groups and six taller palms. Plants retain existing LODs and a measured clear walking corridor.
- Added bounded grass contact around the player, fixed roots and height, matching shadows, darker roots and per-clump variation.
- The selected project uses a brighter canopy look while preserving mist and coherent lighting in geometry, sky, reflections and local probes. Legacy projects retain their original atmosphere.
- Output remains 1600×900. Normal launch starts at full internal resolution and can adjust to 70%; shadows, reflections, clouds and fog remain enabled.

## Verification

Native RX 5300 GPU checks passed for the five-layer texture array, exact pixels, hostile upload state, failure ownership, 16 terrain samplers, 7,680 wind samples, grass contact and matching shadows. The atmosphere checks and 12 packaging tests pass.

The packaged complete journey passes actual defeat/Enter retry, trial and crossing combat, VFX, report, save/load, reward and ending. It recorded 15 hits, 10 parries, 670 trail segments, no VFX failures, GL errors or leftover texture references. Normal PulseAudio startup also passes.

Walking benchmarks use fixed 70% internal resolution (1120×630), 1600×900 output, all listed effects enabled, VSync and no automatic quality changes. No compilation, large asset scans or encoding ran during these intervals.

The table was measured on `bc74a5e7efd58d9e23aeb0b0925fc60a4b8f0f391ab7cc9b84a461fb9513e0c3` (preserved in benchmark-binary/rpg). The final `47a28469e643ac65ccf9d1fe2cb019a4dc4eccdf6b1610c2e8c5e91f5bf08710` build changes only the loading-time particle warm-up; the art and steady render loop are unchanged. The complete journey/retry was repeated on the final binary.

| Camera | Route | Average FPS | 1% low FPS | Frames below 45 | Samples |
|---|---|---:|---:|---:|---:|
| third | rainforest | 55.92 | 44.46 | 53 | 7580 |
| third | castle-west-street | 59.95 | 59.56 | 0 | 10380 |
| first | rainforest | 56.44 | 44.99 | 29 | 7580 |
| first | castle-west-street | 59.95 | 59.29 | 0 | 10380 |

Every route completes without blocked movement or graphics errors. The castle meets both statistical and strict 45 FPS gates. The jungle fails the statistical 45 FPS target and retains brief dips; its worst frame is 24.16 ms. The prior polish build had approximately 57 FPS jungle averages and 45.7–46.4 FPS 1% lows, so the new art has a measurable cost. The performance issue remains open.

Peak system VRAM was 2.973 GB of 3.204 GB, including desktop and loading, sampled every two seconds. About 221 MiB remained at that peak. This is not an exclusive game allocation figure. Avoid importing all texture variants from a character pack at once.

The final build also fixes loading when authored smoke is disabled, moved away or entirely absent. A private visible emitter warms the renderer before play, then restores the authored set and preview clock. Native tests cover empty/distant emitters and invalid-frame ownership. The actual isolated editor scene that exposed the issue now opens with GL0 and zero remaining texture references.

## Current character direction

The protagonist starts as an ordinary father in modern everyday clothes and later wears knight armor. Both appearances must share one face, body and skeleton. The modern starting look, wardrobe transition, new facial/finger rig and facial rendering/animation are not implemented in this package. The final requirement is recorded in HERO_REQUIREMENTS.json beside this report; it is also included in the package.

No paid assets have been purchased. The $100–$300 budget remains available for that shared character and compatible clothing. The previously suggested Medieval Explorer Soldier is not an approved purchase: its modern everyday outfit is unverified.

## Launch and evidence

- Workspace presentation launcher: ../../../../Launch_Valwood_Presentation.sh
- Source visual editor launcher: ../../../../Launch_Valwood_Visual_Editor.sh
- Isolated editable project: projects/valwood_visual
- Package: builds/relocated/Valwood Visual Upgrade 2026-10-30
- Previous r2 presentation package and original world/presentation authoring are preserved.
- Binary SHA256: `47a28469e643ac65ccf9d1fe2cb019a4dc4eccdf6b1610c2e8c5e91f5bf08710`
- Package content ID: `bb941484756594db0219ade5f69a3011477da5dee0349c1f1458f77448580b88`
- Package manifest verification: package-verification.json; workflow: final-packaged-journey/results.json; walking: packaged-routes/results.json; memory: vram-samples.json.

Free material provenance, URLs, sizes and hashes are in project/assets/visual_upgrade/CREDITS.txt and forest_leaves_03.provenance.json. Forest Leaves 03 and Brown Planks 08 come from Poly Haven under CC0; the existing purchased environment and character licenses remain included.

Installed window close-up: [native image](/home/computer/Desktop/final_engine/new_engine/docs/visual-upgrade/2026-10-30/window-detail-fixed/window.png). Jungle bank: [native image](/home/computer/Desktop/final_engine/new_engine/docs/visual-upgrade/2026-10-30/bank-jungle-070/view.png).
