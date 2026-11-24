# Start here: world design

The first session is for learning the editor and exploring what already exists. Begin with a small, saved environment change in a separate workspace. Use [FIRST_ASSIGNMENT.md](FIRST_ASSIGNMENT.md) as the exercise and [EDITOR_GUIDE.md](EDITOR_GUIDE.md) while editing.

## Open an area

1. Extract the complete Landscape ZIP to a **new** local writable folder. Run **Valwood World Designer.exe** from that folder. All accompanying folders are required. Keep older deliveries and their work folders separate.
2. Select **Castle City & Countryside**, **Expanded Jungle**, **Grandstone Stronghold**, or **Apartment & Valois Arrival**. The default is the city.
3. Click **Open Editor**. On first use, the tool verifies and installs the selected seed, then makes an independent working copy. Watch the status message and allow copying and loading to finish.
4. Tour the area before editing. Use F11 to switch between the editor and world preview. Close one engine session before opening another area.

Unity, a developer SDK and a system Python installation are unnecessary. A Windows x64 computer with the GPU manufacturer's OpenGL 3.3 driver is required. The download and extracted bundle are **about 32 GB each**. The selected area's pristine seed and independent work copy require additional storage:

| Selected area | Extra space for seed + first work copy |
| --- | --- |
| Castle City & Countryside | About 31.6 GB |
| Expanded Jungle | About 13.9 GB |
| Grandstone Stronghold | About 49 GB |
| Apartment & Valois Arrival | About 29.3 GB |

Installing all four adds about 124 GB. Reserve about **200 GB for this delivery** to retain the download, extracted bundle, all four seeds/workspaces and working room. Older deliveries and their work folders need additional space. Start with one area if space is limited. **New Workcopy** consumes another full project copy; normal **Open Editor** reuses existing work. The tool checks available space for copying.

## Find and protect your work

The extracted bundle is the delivery. Working data is created beside it:

```text
<bundle folder>_Work/
  Seeds/<area>/             pristine reference package
  Workspaces/<area>/        first editable working copy
  Returns/                  exported saved changes
  Logs/                     frontend operation logs
```

The tool remembers the selected working copy for each area. **Open Editor** reuses it. **New Workcopy** makes an additional timestamped copy from the seed, preserves older copies and selects the new one. **Open Work Folder** opens the current copy in Explorer.

Work inside the selected workspace. Keep the seed unchanged. Each area is a separate project; a camera preset does not create another scene or copy changes between areas. The Apartment & Valois Arrival editor opens outdoors at the latest meadow reference. To inspect the apartment, use **Scene Outliner → Search scene…**, enter `nm_opening_reference_home_story`, select the result, then right-click it and choose **Frame selection**. Fly into the interior after it loads. The separate demo provides the opening story route.

Landscape edits with generated growth use **Studio version 8**. Keep using this Landscape edition's editor, validator and return tools; older editor/game executables cannot open those new saves. This package creates its own separate work folder. The owner reviews returns with the matching new Windows or native Linux runtime/validator, preserving earlier work.

## Build a small landscape

Click **Landscape** at the top, then **Find open ground**. The camera frames terrain away from fixed buildings and imported floors.

- **Shape → Raise / Lower / Flatten / Smooth / Rolling hills:** choose Brush width and Strength, then drag on the ground. Release to build. Flatten uses the height where the stroke begins.
- **Plant → Forest / Meadow / Jungle / Trees / Grass / Plants / Rocks:** choose Brush width and Density, then paint an area. Growth streams around the camera. **Clear growth** clears generated growth; imported foliage remains a normal object.
- **River:** click a short downhill route, then **Finish river**. The editor builds the channel, banks and water together. **Undo last point** fixes a point; **Cancel river** discards the unfinished route.

Painted trees and rocks are visual scenery; they do not automatically block walking. Use collision-enabled individual models or Physics obstacles for solid objects.

Each completed stroke or river has one Undo. Escape cancels the current stroke. Use **Save landscape**, reopen, and inspect before returning. Later painted areas replace the growth mix where they overlap. Keep the first experiment small; there are 32 saved painted areas and 64 sculpt tiles. See [EDITOR_GUIDE.md](EDITOR_GUIDE.md) for limits.

## Save, reopen and return

Object edits normally preview automatically. The simple Landscape tools build after release or **Finish river**; saving is explicit. Materials, imports and advanced terrain tools use **Apply**. **Ctrl+Shift+S** applies and saves the scene; **Ctrl+S** saves the applied scene. Preview and recovery drafts do not replace an explicit save.

After changing separate typed tools, use **File → Save project checkpoint** and wait for completion. Regions have a separate **Save region document** action. Reopen the workspace and confirm the saved result before returning work.

Close the editor, select the same area in World Designer, then click **Return Saved Changes**. It writes a new folder under `Returns/` and opens it. Send that folder, screenshots and the completed [ACCEPTANCE_CHECKLIST.csv](ACCEPTANCE_CHECKLIST.csv) using the agreed delivery method. The tool does not send files automatically. The owner reviews the saved files against the exact matching seed; concurrent changes to the same authoring file require reconciliation.

## First-day boundaries

- Practice with small props, foliage composition, surfaces and lighting. Keep dialogue approaches and walking routes clear.
- Moving, copying or hiding city buildings changes the visible scenery but does not rebuild the separate architectural collision. Record structural proposals for the owner before making them production changes.
- Grandstone's architecture is combined into source chunks. A selected pillar may be part of a larger chunk. Individual module changes require source editing and recooking.
- The opening project already uses all 128 custom asset families. Reuse its existing families for the exercise. New families need a deliberate capacity review.
- Character models, rigging, facial animation and equipment authoring are outside this assignment.

On the receiving Windows PC, complete the startup, controls, save/reopen and return checks in [TESTING_STATUS.md](TESTING_STATUS.md). For help, send the exact steps, area, screenshot, GPU name and the workspace's `logs/editor-latest.log`.
