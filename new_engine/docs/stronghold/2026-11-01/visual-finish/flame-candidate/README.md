# Hall practical-flame candidate

This candidate restores visible fire at the purchased lower hall's existing ten wall torches and six braziers. The current king capture shows empty holders against their strong baked pools of warm light.

Nothing in the production project was changed. `world.studio` here is a complete candidate copy; `particles.txt` contains only the16 added particle rows. Native Studio parsing/validation and re-encoding passed. The production document's SHA256 is recorded before/after in `flames.json` and is identical.

`tools/prepare_showcase_flame_candidate.py` reads the original purchased prefabs and exported scene. It recovers165 authored on-fixtures (54 torch01,68 torch02,31 brazier01,12 brazier02), including nested prefab fixtures absent from the scene's top-level prefab count. All165 source positions are in `flames.json`; only the16 lower-hall fixtures are selected for this candidate.

Each native flame base is the prefab's `Fire_Torch_Embers_01` fuel-mouth origin transformed through the actual exported holder mesh's world matrix. The emitter is not placed at the point light. This distinction matters for braziers: the source flame billboard origin is lower than the fuel mouth, while the light is above it. An independently reconstructed light transform matches every exported point light within1mm; the one source override moves its lightY by1mm. Flame and ember transform IDs and purchased prefab SHA256 values are retained for review.

Examples, native absolute metres:

| Holder | X | Y | Z |
|---|---:|---:|---:|
| Lower-hall west torch at sourceZ646.772 | -43610.1959 | 1287.9465 | -24134.5961 |
| Lower-hall east torch at sourceZ646.866 | -43600.1091 | 1287.9465 | -24134.6902 |
| West brazier at sourceZ633.870 | -43608.3961 | 1284.8275 | -24121.6943 |
| East brazier at sourceZ633.870 | -43601.8391 | 1284.8275 | -24121.6943 |

The native renderer supports56 total flames; existing base emitters occupy35, so this16-emitter candidate totals51. Studio documents allow16 authored effects. Restoring every source fixture will need a separate source pool with bounded nearest/visible selection rather than adding165 Studio effects.

Dynamic light power is0 in this candidate. The architecture already contains the publisher's baked fixture illumination, and the existing particle light channel also illuminates static baked surfaces. `flames.json` records modest proposed actor-only powers (.15 torch/.35 brazier) for a future masked channel; applying them through the current shared channel would duplicate static illumination. The candidate uses the existing flame renderer and shared flicker/sprites, with modest visual intensity. Its actual height, placement, brightness and cost still require a scheduled native visual capture before adoption.
