# Valwood World Designer

Extract the entire ZIP to a writable local folder, then run **Valwood World Designer.exe**. Keep its accompanying folders together. The editor, Python helpers and runtime libraries are included; Unity and a separate Python installation are unnecessary.

Choose an area, then click **Open Editor**:

| Area | What to review |
| --- | --- |
| Castle City & Countryside | Town layout, streets, castle backdrop, meadows and woodland |
| Expanded Jungle | Jungle paths, vegetation layers, wildlife and existing gameplay world |
| Grandstone Stronghold | Snowy approach, mountain entrance, halls and architectural detail |
| Apartment & Valois Arrival | Apartment environment and the latest meadow campsite reference |

The first Open creates a pristine seed and an independent editable workspace beside the extracted bundle. Later opens reuse your selected workspace. **New Workcopy** creates an additional independent copy and keeps previous work. Allow the copy and first scene load to finish before opening another editor.

The extracted bundle is about 32.3 GB. Installing the city seed and first work copy adds about 31.6 GB; other area sizes are in `ARTIST_START_HERE.md`. Reserve about 200 GB to keep the download, extracted bundle and all four independent seeds/workspaces. Installing one area uses much less space.

**Apartment & Valois Arrival opens at the meadow.** To inspect its apartment, search the **Scene Outliner** for `nm_opening_reference_home_story`, select the result, then right-click it and choose **Frame selection**. See the editor guide for camera navigation. This tool opens an editable environment view; the separate demo starts the story.

Start with [ARTIST_START_HERE.md](ARTIST_START_HERE.md), then do the thirty-minute exercise in [FIRST_ASSIGNMENT.md](FIRST_ASSIGNMENT.md). Your first session is for touring the world and learning placement, terrain snapping, saving and returning a small environment edit.

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

City structural collision is a separate resource, and Grandstone architecture is combined into chunks. Read the boundaries in [EDITOR_GUIDE.md](EDITOR_GUIDE.md) before moving buildings or reshaping their foundations. Character creation is outside this environment assignment.

For startup problems, use the frontend status message and logs. Editor logs are in the selected workspace's `logs/editor-latest.log`; frontend logs are beside the bundle in `<bundle folder>_Work/Logs/`. See [TESTING_STATUS.md](TESTING_STATUS.md) for verification scope and the receiving-PC check.
