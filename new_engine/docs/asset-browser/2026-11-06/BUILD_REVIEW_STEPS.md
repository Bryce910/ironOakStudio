Private asset-browser successor build (CPU only)

Parent: accepted collision/water runtime v9 on each platform. Frozen parent files are never edited.

The builder starts PRELIMINARY=True, REVIEWED_SOURCE={}, and no compiler-support SHA. It can inspect the actual inherited object graph but cannot freeze/build until root reviews exact source bytes and compiler evidence.

Expected recompilation graph: world_viewer.cpp, world_designer_assets.cpp, and world_designer_workspace.cpp. The last consumer includes the modified catalog header for preference validation. The project validator has no affected consumer and remains byte-identical. New production translation units are refused.

After root declares the complete source ready:

1. Run independent CPU observations:
   python3 docs/asset-browser/2026-11-06/preflight_runtime_v1.py --platform native --revision final-observe-v1
   python3 docs/asset-browser/2026-11-06/preflight_runtime_v1.py --platform windows --revision final-observe-v1

2. Combine the two successful RESULTS.json reports:
   python3 docs/asset-browser/2026-11-06/preflight_runtime_v1.py --combine docs/asset-browser/2026-11-06/preflight-native-final-observe-v1/RESULTS.json docs/asset-browser/2026-11-06/preflight-windows-final-observe-v1/RESULTS.json

3. Review the final diff against parent source-v9. Pin every exact changed-source SHA in REVIEWED_SOURCE (including SOURCE_PARENT.json metadata and the two new test files), set PRELIMINARY=False, and pin the reviewed compiler-support manifest SHA. Do not add files to the source allowlist without explicit review.

4. Run --strict syntax preflight for both platforms with a fresh evidence revision. This executes the actual executor's postcompile dependency validator against syntax-only .d files and checks all actual private headers originate in the new source root. It creates no object or runtime plan.

5. Root alone freezes each plan with build_runtime_v1.py --platform PLATFORM --freeze, reviews its exact SHA and graph, then supplies --build --go-sha256 SHA. Build receipts preserve parent runtime, art, collision payloads, all shader banks and user scenes. No GPU operation is performed by these scripts.

Existing evidence, plans and runtimes are never overwritten. All UI/GPU acceptance remains root's responsibility after the private build.
