# Start here: world design

The first session is for learning the editor and exploring what already exists. Begin with a small, saved environment change in a separate workspace. Use [FIRST_ASSIGNMENT.md](FIRST_ASSIGNMENT.md) as the exercise and [EDITOR_GUIDE.md](EDITOR_GUIDE.md) while editing.

## Open an area

1. Extract the complete ZIP to a local writable folder. Run **Valwood World Designer.exe** from that folder. All accompanying folders are required.
2. Select **Castle City & Countryside**, **Expanded Jungle**, **Grandstone Stronghold**, or **Apartment & Valois Arrival**. The default is the city.
3. Click **Open Editor**. On first use, the tool verifies and installs the selected seed, then makes an independent working copy. Watch the status message and allow copying and loading to finish.
4. Tour the area before editing. Use F11 to switch between the editor and world preview. Close one engine session before opening another area.

Unity, a developer SDK and a system Python installation are unnecessary. A Windows x64 computer with the GPU manufacturer's OpenGL 3.3 driver is required. The extracted bundle is about **32.3 GB**. The selected area's pristine seed and independent work copy require additional storage:

| Selected area | Extra space for seed + first work copy |
| --- | --- |
| Castle City & Countryside | About 31.6 GB |
| Expanded Jungle | About 13.9 GB |
| Grandstone Stronghold | About 49 GB |
| Apartment & Valois Arrival | About 29.3 GB |

Installing all four adds about 124 GB. Reserve about **200 GB total storage** to retain the original download, extracted bundle, all four seeds/workspaces and working room. Start with one area if space is limited. **New Workcopy** consumes another full project copy; normal **Open Editor** reuses existing work. The tool checks available space for copying.

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

## Save, reopen and return

Object edits normally preview automatically. Terrain, materials and imports use explicit **Apply**. **Ctrl+Shift+S** applies and saves the scene; **Ctrl+S** saves the applied scene. Preview and recovery drafts do not replace an explicit save.

After changing separate typed tools, use **File → Save project checkpoint** and wait for completion. Regions have a separate **Save region document** action. Reopen the workspace and confirm the saved result before returning work.

Close the editor, select the same area in World Designer, then click **Return Saved Changes**. It writes a new folder under `Returns/` and opens it. Send that folder, screenshots and the completed [ACCEPTANCE_CHECKLIST.csv](ACCEPTANCE_CHECKLIST.csv) using the agreed delivery method. The tool does not send files automatically. The owner reviews the saved files against the exact matching seed; concurrent changes to the same authoring file require reconciliation.

## First-day boundaries

- Practice with small props, foliage composition, surfaces and lighting. Keep dialogue approaches and walking routes clear.
- Moving, copying or hiding city buildings changes the visible scenery but does not rebuild the separate architectural collision. Record structural proposals for the owner before making them production changes.
- Grandstone's architecture is combined into source chunks. A selected pillar may be part of a larger chunk. Individual module changes require source editing and recooking.
- The opening project already uses all 128 custom asset families. Reuse its existing families for the exercise. New families need a deliberate capacity review.
- Character models, rigging, facial animation and equipment authoring are outside this assignment.

On the receiving Windows PC, complete the startup, controls, save/reopen and return checks in [TESTING_STATUS.md](TESTING_STATUS.md). For help, send the exact steps, area, screenshot, GPU name and the workspace's `logs/editor-latest.log`.
