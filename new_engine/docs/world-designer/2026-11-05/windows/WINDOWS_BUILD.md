# Windows world designer delivery

Run **Valwood World Designer.exe** from the fully extracted package. It is the native Windows x64 project picker for Valwood's custom SDL/OpenGL editor. Unity, a compiler, a developer SDK and a separate Python installation are unnecessary. Keep the packaged executable, libraries, helpers and content together.

## Included projects

| Picker choice | Label | Initial environment view |
| --- | --- | --- |
| Castle City & Countryside | `city` | `city-walk` |
| Expanded Jungle | `jungle` | `rainforest-milestone` |
| Grandstone Stronghold | `grandstone` | `stronghold-town` |
| Apartment & Valois Arrival | `opening` | `city-meadow` |

The opening project contains the latest apartment and meadow reference data. The editor opens an environment view; use the separate playable demo to rehearse the story introduction.

## Workspace and return behavior

First **Open Editor** verifies/installs the selected seed and creates an independent working copy under sibling `<bundle folder>_Work/`. The seed is in `Seeds/<label>` and the first work copy in `Workspaces/<label>`. Existing selected work is reused. **New Workcopy** creates a separate timestamped copy without replacing earlier work. The frontend remembers the selected workspace for each label.

**Return Saved Changes** exports saved supported changes to a new `Returns/<label>-.../` folder and opens it. Save and close the editor first. The owner reviews the packet against the matching seed; file conflicts are reported for deliberate reconciliation. It is not an automatic entity-level scene merge.

Frontend logs are in `<bundle folder>_Work/Logs/`. Editor logs are in the current workspace's `logs/editor-latest.log`. The root **Guide** button opens `WORLD_DESIGNER_GUIDE.md`.

## Requirements and acceptance

Use local writable storage. The extracted content is about 32.3 GB. An independent seed plus first workspace adds about 31.6 GB for the city, 13.9 GB for the jungle, 49 GB for Grandstone, or 29.3 GB for the opening. All four add about 124 GB; reserve about 200 GB to retain the download, extracted content, all seeds/workspaces and working room. Installing one area needs much less. Additional work copies consume another full project copy. The tool checks copying space requirements.

A GPU manufacturer's OpenGL 3.3 driver is required. Install the appropriate GPU driver if Windows is using a basic display driver.

The editor uses a private runtime derived from the accepted gameplay R11 build, with a repaired scenery transaction byte limit for the expanded jungle. Gameplay executables and projects are unchanged. Windows CPU checks passed: the existing platform tool's 27 checks with real asset import, four-project authoring validation, 14 native picker self-test checks and 28 dispatcher checks pinned to the final editor/helper bytes. A separate CLI check verified Unicode-path status output. The exact editor's copy/paste, terrain-snap and VFX Save/reopen workflows and all four current environment openings passed under Wine on the RX 5300.

The package is released only after streamed CRC/SHA, index, manifest, dependency and runtime verification. A SHA-256 sidecar accompanies the ZIP. See `TESTING_STATUS.md` for the tested scope. The receiving Windows PC still needs its own startup, input/display, small-edit save/reopen and return acceptance. No universal FPS guarantee is made by packaging the editor.

This is an environment design handoff. City structural collision and Grandstone architectural chunks require the separate source/collision workflows described in `EDITOR_GUIDE.md`. Character creation and rigging are outside the assignment. Runtime asset provenance and license notices remain with the delivered data; original purchased source archives are not included in artist returns.
