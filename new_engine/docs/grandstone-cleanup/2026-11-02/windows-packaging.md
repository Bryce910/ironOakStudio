# Grandstone walking and camera fix add-on

The certified artifact is Valwood_Windows_Grandstone_Fix_2026-11-02.zip (13,160,342 bytes; SHA256 e93a574e2c0b58c79c138d37a3e42f431aa72ef04a91b3a166f9ba3fb3b249cb). It is a Grandstone-only add-on built against the original verified orientation ZIP; earlier traversal and combat ZIPs remain historical artifacts. City and jungle can continue using the accepted combat add-on.

Use a fresh extracted orientation transport folder, then extract this add-on there. It supplies playtest-index.json, Install_Playtest_Bundle.py, Install_Playtest_grandstone.bat and new blobs while keeping the original bundle-index.json and orientation data. Create a new independent seed; for example:

```bat
Install_Playtest_grandstone.bat --output "%USERPROFILE%\Desktop\Valwood_Grandstone_Fix_Playtest_Seed"
```

Run Launch_Playtest.bat in that new seed and use PLAYTEST_GUIDE.md and PLAYTEST_CHECKLIST.csv. Existing destinations are never overwritten. No Unity, separately installed Python or new purchased art is needed.

The patch map takes the exact six staged route/sector files and the one accepted private shader (shaders/world_foliage.fs.glsl) from the isolated cleanup QA copy. The shader retains its existing empty dependency closure; its replacement SHA256 is d62cf5aeba53a0f326ded73012bcadc91af522edc4d06605d4f23a9bc3fe5c49. Default source runtime data and the user's existing local project remain unchanged. Packaging and transport certification passed against the accepted final Windows combat results, including all six current runtime files and the accepted private shader. See windows-overlay-build.json, windows-overlay-verification.json and windows-overlay-preservation.json. The final verifier streamed all 18 add-on members and verified complete manifest/database/dependency closure; 3,195 unpatched original Grandstone file records remain exact. Native Windows hardware installation remains a tester-PC acceptance step. Collision/support and rendering claims are evaluated separately by the native/Windows QA reports; the preparer itself proves isolation, not walkability.
