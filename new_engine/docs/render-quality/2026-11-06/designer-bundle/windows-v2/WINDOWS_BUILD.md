# Windows Landscape and rendering editor delivery

This rendering update keeps the Landscape tools, Studio version 8 saves, matching authoring validators and existing return workflow. It adds consistent terrain/model materials, controlled wetness, rooted grass movement and filtered local reflections. It creates a separate delivery and work folder; keep earlier deliveries and work unchanged.

Run **Valwood World Designer.exe** from the fully extracted Landscape package. It is the native Windows x64 picker for Valwood's custom SDL/OpenGL editor. Unity, a compiler, a developer SDK and a separate Python installation are unnecessary. Keep the packaged executable, libraries, helpers and content together.

## Included projects

| Picker choice | Label | Initial environment view |
| --- | --- | --- |
| Castle City & Countryside | `city` | `city-walk` |
| Expanded Jungle | `jungle` | `rainforest-milestone` |
| Grandstone Stronghold | `grandstone` | `stronghold-town` |
| Apartment & Valois Arrival | `opening` | `city-meadow` |

The opening contains the latest apartment and meadow reference data. It starts at the meadow in this editor. Use the outliner method in EDITOR_GUIDE.md to inspect the apartment; use the separate playable demo for the story introduction.

The **Landscape** button opens Shape, Plant and River tools. Terrain shaping supports 64 local tiles; generated growth uses 32 saved areas; rivers build their channel, banks and water together. See FIRST_ASSIGNMENT.md for the short exercise.

## Workspaces and version boundaries

Extract this edition into a **new folder**. First **Open Editor** verifies/installs its selected seed and makes an independent work copy under sibling `<bundle folder>_Work/`. Seeds live in `Seeds/<label>` and the first copy in `Workspaces/<label>`. Later opens reuse the selected copy. **New Workcopy** creates another timestamped copy without replacing earlier work.

Keep earlier deliveries and their work folders intact. This edition includes a matching Windows editor/authoring validator and version-8-aware Python inventory/return helper; the owner's build/review uses the matching native validator. Generated-growth data uses Studio version 8. Do not replace an EXE inside an older workcopy or manually alter its baseline metadata. The owner must validate and deliberately migrate saved work between exact package revisions.

**Return Saved Changes** writes supported saved changes to a new `Returns/<label>-.../` folder and opens it. Save and close the editor first. Review uses the matching seed; file conflicts require reconciliation. It is not an automatic entity-level scene merge.

Frontend logs: `<bundle folder>_Work/Logs/`. Editor logs: `<workspace>/logs/editor-latest.log`. **Guide** opens WORLD_DESIGNER_GUIDE.md.

## Storage, drivers and verification

Use writable local storage and the GPU manufacturer's OpenGL 3.3 driver. The ZIP stores its content without compression for faster packaging; the download and extracted content are about 32GB each. An independent seed plus first workspace adds about 31.6GB for the city, 13.9GB for the jungle, 49GB for Grandstone or 29.3GB for the opening. All four add about 124GB. Reserve about 200GB for this delivery's download, extraction, seeds/workspaces and working room; older deliveries and additional work copies require extra space. The frontend checks copying space.

The package is released only after streamed CRC/SHA, index, manifest, asset-dependency and runtime verification. A SHA-256 sidecar accompanies the ZIP. TESTING_STATUS.md records the exact tested scope. The receiving physical Windows PC still needs startup, mouse/display, landscape/prop save/reopen and return acceptance. These functional checks do not establish sustained FPS on every GPU.

This is an environment-design handoff. City structural collision and Grandstone chunks require the separate source/collision workflow in EDITOR_GUIDE.md. Character creation and rigging are outside this assignment. Asset provenance and license notices remain with delivered data; original purchased source archives are excluded from artist returns.
