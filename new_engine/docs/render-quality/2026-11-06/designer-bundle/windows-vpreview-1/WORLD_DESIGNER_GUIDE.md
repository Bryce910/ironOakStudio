# Valwood World Designer — Landscape and rendering edition

Extract the entire ZIP into a **new**, writable local folder, then run **Valwood World Designer.exe**. Keep its accompanying folders together. The editor, Python helpers and runtime libraries are included; Unity and a separate Python installation are unnecessary. Keep older deliveries and their work folders separate.

Choose an area, then click **Open Editor**:

| Area | What to review |
| --- | --- |
| Castle City & Countryside | Town layout, streets, castle backdrop, meadows and woodland |
| Expanded Jungle | Jungle paths, vegetation layers, wildlife and existing gameplay world |
| Grandstone Stronghold | Snowy approach, mountain entrance, halls and architectural detail |
| Apartment & Valois Arrival | Apartment environment and the latest meadow campsite reference |

The first Open creates a pristine seed and an independent editable workspace beside the extracted bundle. Later opens reuse your selected workspace. **New Workcopy** creates an additional independent copy and keeps previous work. Allow the copy and first scene load to finish before opening another editor.

The download and extracted content are each about 32 GB. Installing the city seed and first work copy adds about 31.6 GB; other area sizes are in `ARTIST_START_HERE.md`. Reserve about 200 GB for this delivery's download, extracted bundle and all four independent seeds/workspaces. Older deliveries and additional work copies need extra space. Installing one area uses much less space.

**To build scenery, click Landscape at the top of the editor, then Find open ground.** Choose **Shape** for hills, **Plant** for forests/grass/vegetation, or **River** for a channel with water. Drag to paint; release to build. Each stroke has one Undo. **Save landscape** applies and saves. The short exercise in [FIRST_ASSIGNMENT.md](FIRST_ASSIGNMENT.md) shows the complete flow. Painted trees and rocks are visual scenery; use collision-enabled individual models or Physics obstacles for solid obstacles.

**Apartment & Valois Arrival opens at the meadow.** To inspect its apartment, search the **Scene Outliner** for `nm_opening_reference_home_story`, select the result, then right-click it and choose **Frame selection**. See the editor guide for camera navigation. This tool opens an editable environment view; the separate demo starts the story.

Start with [ARTIST_START_HERE.md](ARTIST_START_HERE.md), then do the thirty-minute exercise in [FIRST_ASSIGNMENT.md](FIRST_ASSIGNMENT.md). Your first session is for touring, making a small landscape, learning prop placement and returning a saved edit.

Use **Open Work Folder** to find your saved project. After saving and closing the editor, **Return Saved Changes** creates a return folder for owner review. The tool opens that folder; send it using the agreed delivery method.

| Useful controls | |
| --- | --- |
| Editor / play | F11 |
| Fly camera | Hold right mouse + WASD; Q/E down/up |
| Select / move / yaw / scale | Q / W / E / R |
| Frame selection | F |
| Copy / paste / duplicate | Ctrl+C / Ctrl+V / Ctrl+D |
| Snap to terrain | End |
| Undo / redo | Ctrl+Z / Ctrl+Y |
| Apply and save | Ctrl+Shift+S |

Buildings, streets, existing routes and imported ground have protections. Use **Find open ground** when a brush is blocked. City structural collision is a separate resource, and Grandstone architecture is combined into chunks. Read the boundaries in [EDITOR_GUIDE.md](EDITOR_GUIDE.md). Character creation is outside this environment assignment.

For startup problems, use the frontend status message and logs. Editor logs are in the selected workspace's `logs/editor-latest.log`; frontend logs are beside the bundle in `<bundle folder>_Work/Logs/`. See [TESTING_STATUS.md](TESTING_STATUS.md) for verification scope and the receiving-PC check.
