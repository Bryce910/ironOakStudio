# Focused visual milestone — 30 October 2026

The first playable art pass is implemented in `projects/valwood_visual`. It is ready for visual review in the running source build. This is not a claim that the game or the entire atlas now meets an AAA art standard.

## What changed

- **Watch courtyard:** scanned 2K cobblestone PBR paving with the correct DirectX normal convention for the terrain loader; stained and worn ground around the well; buckets, seating and four planted stone beds; folded cloth banners with a raised gold crest; seven castle wall slit assemblies fitted to the measured inner stone face. Sixteen earlier village facade assemblies remain. Eight placeholder archers in this courtyard become two editable knight sentries; Corvin and the combat trainees retain their identities and bindings.
- **Short jungle path:** photographed canopy and broad-leaf understory, clustered grass, ferns, roots and the existing leaf-litter terrain. Two LODs for each new plant. Canopy detail switches at 28m and authored grass at 10m, with grass ending at 45m. The source 1.6M-triangle canopy was reduced to 25,399 triangles near / 9,973 mid. The opening stump was moved onto the bank after the actual walking test found its footprint blocking the path.
- **Current armored hero:** an in-game portrait of the actual player body, with orbit, framing controls, authored fill/rim lights and opt-in depth of field. Normal walking post-processing is unchanged when portrait focus is disabled. The restored 2K armor maps from the preceding pass are retained.
- **Playable route:** the journey now begins in this courtyard. Paving and banners also carry through the selected project's wider village. The focused jungle section covers the entry and the first main trail segment; it does not replace the entire forest.

The private scene contains 8156 / 8,192 scenery records. Authoring remains editable. The shared original world and the previous presentation packages were preserved. The rainforest source proxy was made private before moving its opening stump, retaining the original record number.

## Review controls

Open `/home/computer/Desktop/final_engine/Launch_Valwood_Presentation.sh`. It now prefers this updated source build. Choose **Visual showcase** or press **F7** from the title, pause menu or ordinary play outside combat.

- **1** Courtyard; **2** Jungle path; **3** Hero portrait.
- **WASD**, right mouse and **V** for the two walking views.
- Right mouse orbits the portrait; the wheel adjusts framing.
- **Esc** returns to the previous camera and presentation screen.

The showcase suspends gameplay updates and saves. Its native input test verified all three views, orbit/zoom, prevention of disappearing-player toggles, quest counter isolation, return and re-entry; pressing F5 in review created no save.

## Captures

All three were captured on the RX 5300 at 1600×900 output **and internal resolution**, automatic scaling off. All exited successfully with GL0, zero remaining texture references and zero failed scenery streams.

[Courtyard](review-images/court.png) · [Jungle path](review-images/jungle.png) · [Armored hero](review-images/hero.png)

## Walking measurements

Native RX 5300; 1600×900 output / 1120×630 internal (`render_scale=.7`); automatic scaling off; VSync and all effects retained. No compilation, captures, encoding or asset scans during the walking intervals.

| Camera | Route | Mean FPS | 1% low FPS | Worst ms | Frames below 45 FPS |
|---|---|---:|---:|---:|---:|
| third | Jungle path | 55.47 | 41.88 | 24.68 | 126/1980 |
| third | Courtyard | 59.95 | 59.09 | 17.68 | 0/880 |
| first | Jungle path | 56.07 | 42.25 | 25.22 | 68/1980 |
| first | Courtyard | 59.95 | 59.38 | 17.00 | 0/880 |

Both camera modes completed the 41.969m courtyard loop and 97.846m jungle return with zero collision/slope blocks and zero return-position error. **The courtyard passes the strict 45 FPS floor; the jungle does not.** The later lighter procedural-tree comparison produced no meaningful improvement and was not adopted. These short focused routes do not certify the complete forest's sustained performance.

Peak aggregate device VRAM during these runs was 2,949,709,824 bytes, including the desktop. Source graphics defaults still start at native internal resolution and allow automatic scaling to .7.

## Functional checks

- Native complete-journey rehearsal: **passed**, including actual defeat/retry, directional combat, VFX, memory, checkpoint save/load, rewards and ending. 15 hits, 10 parries, 16 dust bursts and 670 trail segments; no VFX failures. This rehearsal preceded the one entrance-stump clearance correction, which the subsequent complete walking runs verified separately.
- Native showcase input workflow: **passed**.
- Native production portrait shader and camera checks: **passed**. Focus off matched the preserved shader exactly; in-focus pixels matched exactly, background stripe contrast fell from 184.515 to 23.201, depth texture unchanged, GL0.
- Project authoring validation: **passed**.
- Native three-view checks: **passed**.

Executable SHA-256: `9843c03c1dd35fb49ed4342eb41dacc3be8cb41ac016dc5a0dec3a42056003f7`. Full evidence is in `walking-verified/`, `journey-final/`, `workflow/`, `review-images/` and `portrait-final.log`. Older failed captures and route attempts remain as diagnostic evidence; they are not readiness results.

## Remaining content and delivery gaps

The uncovered modern father, his everyday clothing and a matching high-quality knight identity still require a replacement source character. No facial animation, lip sync, finger animation or acted lines were added; recorded dialogue remains 0/4. The current portrait presents the existing helmeted knight. No paid assets were bought.

The jungle needs another performance pass to maintain 45 FPS through every measured interval. The wider village/forest still need art work beyond this focused section. A fresh standalone package has not been built: the existing package is about 6.7GB while available disk space is about 3.5GB. The updated source build is the presentation launcher’s current target, and the older packaged release remains available.

## New asset sources

Poly Haven CC0: [Island Tree 01](https://polyhaven.com/a/island_tree_01), [Calathea Orbifolia 01](https://polyhaven.com/a/calathea_orbifolia_01), [Fabric Pattern 07](https://polyhaven.com/a/fabric_pattern_07), [Cobblestone Floor 08](https://polyhaven.com/a/cobblestone_floor_08). Downloads were checked against the official API’s MD5/size records; provenance and SHA-256 records are under `assets/polyhaven/milestone` and the selected project's `assets/visual_milestone`. [License](https://polyhaven.com/license).

Rebuild recipes: `tools/build_milestone_assets.py` and `tools/compose_milestone.py`. `tools/check_milestone_views.py` and `tools/check_presentation_routes.py --milestone` reproduce the native review and walking checks.
