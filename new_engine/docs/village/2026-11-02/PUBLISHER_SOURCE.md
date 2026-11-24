# Purchased village reconstruction

The city uses the already owned **Medieval Village Environment** pack referenced by the user. No additional assets were purchased. Original archives, the UE map, source meshes, and source material images remain unchanged.

The supplied UE4.26 map was recovered directly as data, without running Unreal. Its original layout contains 123 static mesh families, including a sky dome and fog helper. The native reconstruction represents 121 visible static families, **97 houses**, **2,840 foliage instances**, and **48 Blueprint rope/cap components**. This represents 4,076 original components, with all 11 supplied camera transforms resolved. The six main house families retain their original 40/37/3/14/1/2 instance counts.

Original actor transforms, source pivots, UVs, tangents, material-slot bindings, material overrides, and 115 components' painted colors were recovered. The paint consists of 22 unique variants. Explicit per-LOD material permutations correct differences between the vendor FBX and UE slot ordering. Small decorations are grouped into 12 metre cells while houses and large structures retain their original placements.

The final isolated candidate is `assets/purchased/medieval_village/publisher-backdrop/catalog.json`: **73 definitions, 275 placements, 143 unique GLB files**. The 69 reconstructed definitions and 203 original placements remain identical to the preceding corrected source catalog. Native CPU validation covers those original 138 GLBs and all five new unique GLBs. The generated zero-area pivot datum has its own ordinary material, preserving a shared HIGH/MID grounding origin without requiring facade attributes.

## Explicit new dressing

The supplied UE map does not contain the separate ALL-format central castle or the combined ribbons mesh. These were not described as recovered original map placements.

- **70 new pennants** use the original three modular ribbon meshes and original two-sided material, including its linear 0.5 base-color factor. Each attachment follows one of the 24 recovered Hermite rope segments. The separately supplied combined ribbons mesh was retained as an unplaced library asset: its identity coordinates do not align reliably with the map's ropes.
- **One new distant castle placement** uses the already owned `SM_Castle` library model at source-native translation `[-134.5, 68.87494140625, 202.6]`. Its bottom sits at source height 35 metres.
- **One new rocky supporting hill** follows the castle's actual bottom support hull. It provides a level approximately 111 × 104 metre support, irregular wider skirts, and 90 overlapping original rock meshes. The existing rock PBR material is reused; enlarged rocks repeat their UVs to retain stone grain. Its near/MID geometry is 33,160/24,080 triangles.

The castle and hill are **visual backdrops**. They add no collision, entrance, interior, or claim of walkability. The existing playable city collision and routes remain separate.

## Narrow source limitations

The ALL-format FBX merges the distinct UE `Roof2`/`Roof2_2` face assignments on House One and House Five; canonical source Roof2 is retained rather than guessing missing face identities. Two painted house MID reductions differ between the supplied UE and FBX versions, so those variants retain their original painted HIGH geometry at MID distance.

Source fog-card shader behavior and ribbon FlipbookWind are not reproduced by this static geometry cook. Rope dynamic UV phase parameters are recorded; original UV0 and source material tiling are retained. The native lighting and visible sky pass is managed separately from the immutable source geometry catalog.

The source BuiltData audit recovered 1,364 registry records and matched all 1,364 component/LOD GUID bindings. However, **1,349 records, including architecture and ground, are analytically almost black**. The 14 nontrivial records belong to foliage. Both current source Sun and SkyLight are Movable. Consequently, no large per-instance architecture GI duplicate was applied: source dynamic sky diffuse should remain active. The bounded decoder and diagnostic ground candidate are retained as audit artifacts, not integrated city content.

Machine-readable hashes, counts, CPU-validation paths, and source-lightmap evidence are in `publisher-source-provenance.json`. Root-owned native captures determine the final visual composition; this report records provenance and CPU checks rather than claiming visual approval or a frame-rate target.
