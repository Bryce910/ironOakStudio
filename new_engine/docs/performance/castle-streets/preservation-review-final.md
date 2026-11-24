The stored [final freeze](native-full-water-warm/source-freeze.json) preserves
all **7 protected inputs** from [the street-work baseline](baseline/preserved-inputs.json)
and all **508 original model-directory files** from the
[bandit-era freeze](../tanthoon-bandits/native/source-freeze.json). There are
zero missing or changed originals. The current model directory has 520
fingerprinted files, including the 12 intentional tavern/shelter/bedroll/seated
additions. Saved graphics preferences match their baseline hash and byte count.

Compared with [the preceding water-ready freeze](native-water-ready/source-freeze.json),
all **520 model files, 57 shaders, 30 scenery files, and 19 ground files** are
unchanged. Both inventories contain 807 files: 804 hashes match and exactly
three differ—`engine/world_viewer.cpp`, `rpg`, and
`tools/check_castle_streets.py`. No fingerprinted paths were added or removed.

[The JSON evidence](preservation-review-final.json) records both hashes for
each changed file and the exact comparison counts. This review compared saved
hash strings only; it did not reread or rehash runtime assets, build, test, or
launch the GPU. The final native batch was still running and had no
`results.json` at review time, so this preservation result makes no full-batch
completion or frame-rate claim.
