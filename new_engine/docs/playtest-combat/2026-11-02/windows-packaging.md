# Windows combat add-on preparation

This version adds visible armed-hero and repeatable sword-combat playtests for all three regions. Its patch map is separate from the earlier traversal release; the original orientation ZIP, traversal ZIP and frozen guides remain historical artifacts.

The new ZIP is built against the original verified orientation ZIP, not against the prior playtest ZIP. Extract it into a fresh extracted copy of that orientation bundle to avoid confusing old and new playtest indexes. The installer always creates a new seed and refuses an existing output. To keep an older seed, supply a different output folder, for example:

```bat
Install_Playtest_city.bat --output "%USERPROFILE%\Desktop\Valwood_city_Combat_Playtest_Seed"
```

The installer filename is unchanged because the package format and safe materialization rules are unchanged. All seed guides and checklists in this version describe the new combat controls. No new purchased art is required: the hero, sword and opponent assets already exist in the orientation payloads.

The new ZIP is now certified against all three successful final Windows combat acceptance runs. See windows-overlay-verification.json for full transport, project/database closure, runtime and section-route checks; windows-overlay-preservation.json verifies the prior traversal ZIP and guides stayed unchanged. The archive is 9,781,433 bytes with SHA256 12b31ff4e06226d608d997e31daa93499affe7d300196ca9df6844b0c5a19369. Full native-Windows hardware installation remains a tester-PC acceptance step; Wine tests do not replace it.
