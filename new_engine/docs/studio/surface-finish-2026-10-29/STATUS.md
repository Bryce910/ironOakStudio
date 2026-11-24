# Surface completion status — 29 October 2026

The surface tools are connected to Studio and the live renderer. The combined native surface workflow passed phase 7 with zero GL errors and zero texture-reference errors: successful Apply, three expected rejections, undo, save/reload and unchanged source sky/roof/canopy fields. The original failed GPU run and timeout retry remain available in workflow/ and workflow-retry1/; they are not passes.

Latest CPU contracts passed for authoring/codec, packed images, immutable imports and road/river terrain fields (cpu-contracts.log). Existing Studio and runtime-editor tests passed after schema 5 integration. The native full-feature fixture compiled production model, terrain, water, shadow, dynamic and skeletal variants, checked actual image/paint/projection pixels and stayed within 15 active model samplers on the RX 5300 (16 available). Normal-map mip reduction preserves mean-vector length so lost normal variance increases roughness instead of producing sharp distant specular flicker.

Implemented: Studio 5 persistence (reads 1–5); surface codec 2 (reads 1–2); immutable image import/revision management; reusable PBR layers and normal-Y convention; terrain paint/erase; oriented alpha/PBR projection boxes; model material blends; fragment image graph nodes and terrain/water/NPC targets; bounded road grades/paint and river channels/meshes; CPU water/navigation queries; local wind shared by vegetation color/shadow/reactivity, water and particles. Studio tabs 22 and 23 expose the new controls. River rendering is a water shader over an authored channel, not a fluid simulation. There is no interactive ripple solver.

## Visual work and evidence

Purchased source masonry was restored with exact geometry checks and backups on castle/gate/wall. **The purchased castle is not instantiated in the demo.** The active keep is village_keep, which already has roughly 417 texels/m stone and source PBR. Only the restored purchased walls/gate affect the live town. Initial courtyard captures show small changes and are not evidence of a 60-fold live-keep improvement. Historical middle-LOD plaster normal/ARM channel blends have not been recooked.

A real CC0 overcast HDR panorama was imported with source/license/provenance preserved. It is normalized against measured upward irradiance. Staged lighting changes preserve all geometry, placement, material, decal, rain, wind, fog and sharpness rows. Four first-person native castle/tavern comparisons passed on the same binary with GL0/ref0 and protected world/settings unchanged (look-compare-castle/results.json). Street shadow readability improved; the tavern retained warm fire/lantern light. These images still show repeated assets, sparse street dressing and simple character silhouettes; they are not evidence of finished AAA art.

The chosen cloudy-readable-hdr look uses sky fill 1.50, sun 0.85, exposure 1.08, hour 16.1, bearing −20°, cloud 0.55, environment intensity 0.274162483 and HDR blend 0.72. A biome gain dims HDR and sky consistently in the jungle. Both jungle original/HDR captures passed on build6, GL0/ref0, with protected files unchanged. The reviewed result retains fog, cloud 0.96 and sky attenuation 0.74, dark tree cover and less pond glare. **The look is now published:** look-published/publication.json records an exact original backup, byte-identical nonlook/environment rows and unchanged graphics settings. Published world SHA256: c492beda65b4b0bb2dee6ee14f93dacbb0450c270ab6cca25a67a191845ebffd.

## Performance and final delivery

Short stationary captures and editor Apply tests are not walking-FPS evidence. Prior full-route baseline: jungle 58.31 average / 46.85 one-percent-low, 8 of 7,580 sampled frames below 45; castle/tavern 59.81 average / 42.16 one-percent-low, 47 of 10,380 below 45 (worst 44.76 ms). Both completed correct routes with GL0/ref0/collision0. That baseline does not satisfy a strict 45 FPS floor. Complete sequential routes must be repeated after the final combined code/look freeze; retain every miss and failed/partial run.

First combined build8 routes completed at the published HDR look and unchanged Custom quality (1600×900 output, 1120×630 internal, automatic quality off). Jungle: 57.918 average / 44.816 one-percent-low, 27/7,580 sub-45 frames, worst 24.849ms. Castle/tavern: 59.876 average / 47.866 one-percent-low, 10/10,380 sub-45 frames, worst 25.327ms. Both completed full correct return routes without GL/reference/collision errors and protected inputs remained unchanged. Castle passes the statistical target; jungle does not. **Neither passes the strict no-miss 45FPS floor.** Original evidence is retained at docs/production/2026-10-29/live-world/routes-final/results.json. These earlier runs are retained rather than replaced.

Final integrated build8 is SHA256 b01f0fdef228a765029cf506cee2b8c8dcb0cc1174e31485c88aeadfbc1ae65d. It passed the production editor workflow (phase9, 39 checks, five commits and one intentional rejection), live-world interactions (phase8, training restart, physics restore, jungle streaming/dialogue), the surface workflow (phase7, three Applies and three rejections), the new live-component tool capture and shared vertex-graph geometry fixture. All native checks reported GL0/ref0. Evidence is under docs/production/2026-10-29/live-world/native/.

The coordinated production chat owns final walking performance and relocated-package verification. The final optimized package has verified and passed a relocated launch; see the final delivery paragraph below. The older v4 package remains only a baseline. No commercial-engine-completion or universal 45 FPS guarantee is claimed here.


## Final optimized walking result

Native old/new comparison proved exact output equivalence for the two shader optimizations: sky 768 cases with zero linear/RGB8 difference, and foliage lighting 1,280 cases / 10,485,760 HDR+indirect+coverage components with zero difference. The changes skip fully replaced sky calculations and zero-contribution backlit GGX work; no geometry, texture, fog, cloud, reflection, shadow or saved quality reduction was made.

Both complete third-person walks passed correctness and zero GL/reference/collision-error checks at the published look. Settings remained 1600×900 output / 1120×630 internal (70%), automatic quality off. Final test setup had Firefox closed under the earlier explicit user permission; Steam, Discord and ChatGPT stayed open. Browser state differs from the first final routes, so all gains cannot be attributed solely to shader optimizations.

| Route | Distance | Average FPS | 1% low FPS | Frames below 45 | Worst frame |
| --- | ---: | ---: | ---: | ---: | ---: |
| Jungle return | 432.63m | 58.039 | 45.049 | 25 / 7,580 | 25.588ms |
| Castle streets / tavern return | 585.07m | 59.887 | 49.732 | 2 / 10,380 | 22.991ms |

Both pass the statistical 45–60 target. **Both fail the strict zero-miss 45FPS-floor gate.** Slow-frame streaks were one frame. Evidence is retained under docs/production/2026-10-29/live-world/routes-optimized/. The final optimized package has now verified and passed the relocated launch described below. Earlier packages remain baselines.


## Verified deliverable

Final package: /home/computer/Desktop/final_engine/new_engine/builds/relocated/Valwood Live World Demo 2026-10-29. Run launch.sh; F11 opens Studio. Package inventory: 1,441 files, approximately 6.48GB, content ID 85113dbec60faacc20a381c95fc93dd96d856ad9b8fefda7b5ef4f6953a729aa. Authored NPC memory profiles are included while personal save state is excluded. Source provenance and licenses are preserved.

The original package path was absent during the relocated smoke, which ran from /tmp. It exited normally: GL0/ref0, six skeletal actors, two physical props, 194 resident/wanted groups, zero pending/failed groups and protected files unchanged. Evidence: docs/production/2026-10-29/live-world/native/package-final/result.json. The current first-person street capture is castle-street.png in the same directory. This smoke verifies runtime correctness/relocation, not walking FPS. The final combined report and readiness.json remain the authoritative cross-system result.

This bounded surface implementation and delivery pass is complete. It does not claim that remaining art repetition, character motion, a fluid solver, independent point-light shadow maps, or the measured isolated frame dips are finished.
