# Market street visual demo verification

A bounded 44-metre section of Market Row now has two full-size covered source shops, three overhead cloth lines, six merchant hooks/lamps, four layered workstations, supported goods and delivery stacks. **135 appended hero objects** preserve the previous 2,150 district records. Twelve resident broadleaf trees and 24 ferns form four backdrop clusters. The final district has **2,321 objects**, with **4,932 combined scenery placements**, 128 people and 12 pets.

Two source-measured paving aprons cover the shops and overlap the existing road by 0.469 m. Local wheel wear, burnished walking paths, damp drainage and sediment use the existing 2049-square ground field, with eight approaches derived from actual source building sections. The terrain height, collision surface, global vegetation density and 16-sampler terrain path are unchanged. Ground history is 16.016 MiB base / 21.35 MiB including mips.

The selected afternoon look uses hour 16.05, bearing -18 degrees, sun 1.12, sky fill 1.50, exposure 1.28 and grade 0.30. It applies once at spawn and remains editable. Castle demo launches use **3072-square shadows with nine soft-shadow taps**, approximately 20 MiB more shadow storage than 2048-square maps. The existing directional roof-volume and ground-return lighting remain in use; no new GI/probe pass was added. Native comparisons rejected two darker lighting variants before selecting this one.

## Current native measurements

Measured sequentially on the **AMD Radeon RX 5300 at 1600x900, capped at 60 FPS**, with full crowd, pets, material, vegetation and lighting paths enabled. No builds, cooking or other heavy CPU checks ran during timing. Source/runtime hashes remained unchanged. Captures were taken outside the measured frame sequence.

| View / route | Mean FPS | 1% low FPS | p99 ms | Worst ms |
|---|---:|---:|---:|---:|
| hero-entry | 59.88 | 52.01 | 18.606 | 20.865 |
| hero-mid | 59.95 | 54.81 | 17.795 | 19.012 |
| hero-reverse | 59.81 | 51.75 | 18.748 | 22.493 |
| market | 59.95 | 57.97 | 16.773 | 21.651 |
| housing | 59.34 | 50.54 | 19.275 | 21.945 |
| hall | 59.95 | 59.56 | 16.726 | 17.260 |
| hero-route | 59.21 | 49.41 | 19.067 | 23.058 |
| demo-route | 59.81 | 52.44 | 18.127 | 27.428 |

All eight runs pass the 45 FPS mean / 1%-low / p99 targets across **29,289 measured frames**. Six isolated frames exceeded 22.22 ms; worst 27.428 ms. This is not a guarantee that every frame stays above 45 FPS.

Both the **88 m hero return walk** and **1,011.5 m district return walk** completed at 3.5 m/s, returned exactly to their starts, and had no collision blocks, slope blocks or sliding steps. All eight runs had zero GL errors, failed tiles, travel-loading frames, unmanaged image uploads or remaining texture owners/references. Human and pet GPU pose interpolation remained enabled.

Peak observed **whole-system** Radeon VRAM was 2,992,967,680 of 3,204,448,256 bytes, leaving approximately **201.7 MiB**. The 1 Hz samples include the desktop and other applications; they are not engine-only memory. Hero props and grove reuse resident model/image families rather than increasing global vegetation density.

## Validation and visual review

The final scene, generated paving data, recipe/model hashes, independent measured canopy bounds, source triangle supports, cloth/eave clearance, full road clearance, original placements, both route arrays, camera movement, project launches and terrain fields pass CPU checks. The focused acceptance suite has **24 tests**, including missing/shrunken aprons and stale generated paving negatives. Full actual-scene crowd and pet checks passed earlier in this same pass; the final apron correction changes paving only.

All three hero angles were reviewed from unmodified native captures after the final ground recook. The remaining source art is visibly stylized in places, including characters and goods; this pass does not certify AAA image quality. The receiving hall and the wider world retain their existing art scope. These measurements cover the listed views/routes, not every part of the 100 km world.

[Complete verification](verification.json) · [Native measurements](native/results.json) · [Frozen hashes](native/source-freeze.json) · [Scene acceptance](scene-acceptance.json) · [Demo controls](../../CASTLE_DEMO.md)

![Hero street, unmodified native capture](captures/hero-entry-view.png)

![Mid-street shop paving and displays](captures/hero-mid-view.png)

![Reverse street and tree backdrop](captures/hero-reverse-view.png)
