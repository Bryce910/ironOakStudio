# Character finish and recordings

The existing character is a 24-joint body rig. It has head, neck, hand and forearm controls, but no jaw, eye or finger joints and no facial morph targets. This pass improves the conversation camera, head attention, wrist/chest gestures and phrase timing. Facial expression, lip sync, individual finger posing and bespoke hero art are still asset/pipeline work; they are not represented as complete.

No actor recordings were found. `voice-recording-script.csv` contains the exact four dialogue texts, character directions and required filenames. Supply human recordings as mono or stereo PCM WAVs under the presentation project's `audio/presentation/dialogue/` directory:

- `730011.wav`: Corvin's introduction.
- `730012.wav`: Corvin's homecoming recognition.
- `730013.wav`: Corvin after the completed oath.
- `730021.wav`: Rowan's report.

Each source file is limited to 8 MiB; the decoded bank is limited to 32 MiB. The player preloads the bank at startup, plays once per dialogue entry, pauses on menu/focus loss, and stops on dialogue close/change. Missing or corrupt clips retain text dialogue. Test fixtures are never packaged as voices. Rebuild the versioned package after supplying recordings; the old package is preserved.

A facial finish needs a compatible skinned face with authored expressions or facial bones, usable finger joints, reference performances and actor audio. These controls must be connected and visually validated on that asset before claiming facial/lip/finger animation.
