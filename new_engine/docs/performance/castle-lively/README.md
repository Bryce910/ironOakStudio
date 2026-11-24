# Livelier castle market verification

The scene now has **128 people, six cats and six Labradors**, with 142 additional market objects. Eight distinct counters, two covered stalls, two loaded handcarts, four banners and 86 supported goods form 14 authored groups. Ten added traders stand at those displays. The district has 2,150 scenery objects; combined protected scenery has 4,761.

Measured on the AMD Radeon RX 5300 at **1600×900**, capped at 60 FPS, with the full crowd, pets, lighting and ground materials enabled. Source and runtime asset hashes remained unchanged; no builds or cooking ran during measurement.

| View / route | Mean FPS | 1% low FPS | p99 ms | Worst ms |
|---|---:|---:|---:|---:|
| housing | 59.74 | 52.50 | 18.579 | 19.622 |
| marketrow | 59.95 | 59.43 | 16.740 | 17.167 |
| market | 59.95 | 58.99 | 16.730 | 19.260 |
| stalls | 59.95 | 59.54 | 16.732 | 16.992 |
| forecourt | 59.95 | 59.29 | 16.728 | 18.253 |
| hall | 59.95 | 59.61 | 16.718 | 17.241 |
| demo-route | 59.90 | 54.51 | 17.583 | 27.310 |

All seven runs passed the 45 FPS mean/1%-low/p99 targets across **27,600 measured frames**. The complete 1,011.5 m walk returned exactly to its start with no collision blocks, slope blocks, sliding steps, missing tiles, GL errors, unmanaged texture uploads or remaining texture owners. Two route frames exceeded the 22.22 ms budget; worst 27.31 ms. Other resolutions and world locations need their own measurements.

The 600-second CPU crowd proof checks 292,608,000 human pairs and 4,608,000 collision/ground probes. The combined human/pet test checks 55,297,536 pairs. All paths and supported market goods pass the final actual-scene geometry checks. Native population and animal GL tests pass with the real source meshes, pose interpolation, matching shadows and complete cleanup.

The pets add 32 baked poses, three shared image policies (12 MiB conservative RGBA+mips) and 8,426,944 geometry bytes. They use bounded offline motion and no runtime skinning. Market additions introduce no new model or texture families.

Observed peak **whole-system** Radeon VRAM was 2,991,247,360 of 3,204,448,256 bytes, leaving approximately 203.3 MiB. Samples include desktop and other applications at 1 Hz; this is not engine-only memory.

[Complete acceptance](verification.json) · [Native measurements](native/results.json) · [Frozen source hashes](native/source-freeze.json) · [Animal credits](../../../assets/purchased/pets/PROVENANCE.json)

The castle district remains a visual/traversal demo; dialogue, trading, quests and combat remain outside its current scope. Cats use a textured Bicolor Cat source with an authored offline four-paw walk. Labradors use purchased source animation clips.

![Market and cat, actual native engine capture](captures/cat.png)
