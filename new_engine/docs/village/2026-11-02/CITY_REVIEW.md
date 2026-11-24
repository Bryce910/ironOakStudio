# Native city review — 2 November 2026

The owned Medieval Village Environment now runs in the separate `valwood_city_showcase` project. It uses the recovered 97-house source layout, sloped ground, authored material paint and market dressing, with the source visible HDR sky, corrected plaster/ground blends, brighter diffuse fill, 70 rope-fitted pennants, and a distant castle on a rocky foundation. Original main-game saves and the previous Grandstone build were retained.

All 11 source viewpoints passed native AMD capture checks with zero GL or streaming failures. All 146 model bindings passed CPU preparation validation; eight actual Jolt checks cover connected market/square lanes, spawn clearance and a blocked wall crossing.

The separate 56-metre render-route test measured **57.48 FPS average and 43.32 FPS 1% low** at **1600×900, 100% render scale**, on the RX 5300. It recorded 1,584 motion frames, four below 45 FPS and one below 30 FPS. No screenshot readbacks or walking probe bakes occurred during measured motion. This is camera replay over independently checked lanes, not a full gameplay stress test or a locked frame-rate guarantee.

Open `/home/computer/Desktop/final_engine/Launch_Valwood_City.sh` to walk through the review. The project has private saves and graphics preferences. The original main quest/vendor coordinates have not been moved into this layout. Castle and hill are visual backdrops; the source buildings do not supply furnished interiors. Hero replacement remains deferred as requested.

See `final-native/` for actual game captures, `final-timing/result.json` for timings, `publisher-source-provenance.json` for source fidelity, and `CITY_REVIEW.json` for the compact evidence summary.
