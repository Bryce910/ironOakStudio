# Recessed exterior keep

The walkable keep retains its four round towers, conical roofs, 36 × 35 × 28 m envelope, tower centres, all 14 collision boxes, and the full 4.6 × 6 m rectangular entrance. The exterior is rebuilt with actual arched wall apertures and 0.32 m stone reveals, opaque recessed timber shutters, cut-stone voussoirs and sills, defensive lower slits, two upper hall levels, and a high gable triplet. Chamfered stringcourses and corbel-supported tower crowns provide architectural depth. Existing floor, ceiling, and four interior support locations remain; this pass does not furnish an interior.

| Contract | Near | Mid |
| --- | ---: | ---: |
| Triangles | 30,431 | 12,739 |
| Material draws | 4 | 4 |
| Actual recessed apertures | 77 | 65 |
| Portal clearance rays | 323 | 323 |
| Old collider records retained | 14 | 14 |

The mid model simplifies curved subdivision, roof courses, corbels, and outward-facing tower slits. All retained windows still have real depth. Window backs are 0.32 m behind masonry; timber mullions are 0.275 m behind it. Every generated triangle has finite attributes, nonzero area, unit normals, and winding agreeing with its normals. CPU rays test every aperture, adjacent intact masonry, and the entire doorway including its upper corners. Added crossbar and flat-window negative controls exercise the tests.

The four standard glTF materials use the exact encoded stone and plain timber base/normal/ARM channels from `purchased_village_house_a_near.glb`, plus the retained CC0 slate roof channels from the prior keep. Dark shutters share the same timber images. The roof receives a modest cool gray factor. Images and samplers are unchanged, and the geometry adds no image policies to the live scene. The purchased texture license remains in force; the new derivative asset is not labelled wholly CC0.

The prototype-kit validator retains its 128 MiB private-image cap. It separately reports the full kit subtotal and credits only the six exact purchased stone/timber channel hashes after comparing their encoded bytes, sampler settings, and texture coordinates to the actual resident donor. That cross-kit credit does not raise the cap or conceal total unique bytes. The live world’s full image union is checked by the runtime integration tests.

The isolated cooker is `tools/build_village_keep_hero.py`. Staging and validation do not run the older prototype cooker or alter scene placements. Publication replaces only `village_keep.glb`, `village_keep_mid.glb`, and their two manifest rows. The shared kit validator then refreshes diagnostics and aggregate summary. Other kit asset bytes, collision records, material provenance, and license fields remain unchanged.

To reproduce after intentionally cooking the original prototype kit, run:

```sh
python3 tools/build_village_keep_hero.py --stage
python3 tools/build_village_keep_hero.py --publish
python3 tools/build_castle_village_assets.py --validate
python3 tools/test_village_keep_hero.py
```

The standalone baseline under `assets/purchased/keep_hero/before` is retained so subsequent runs use the exact original slate image bytes and collision records. Do not replace that baseline with a later derivative. After model publication, refresh the static scene/model hashes and the village ground proof through the usual scene authoring workflow.

Proofs and CPU-only front/three-quarter previews are in `docs/performance/village-hero-keep`. These previews verify architecture and materials under a common offline light; they are not in-engine captures or performance evidence. Native village QA remains necessary before judging the finished lighting and frame rate.
