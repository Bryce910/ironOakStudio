# Guided walking route data

These small files add ordered wayfinding to the disposable playtest copies; they do not change the protected source projects, move the player automatically, or add new quests. The engine integration and native acceptance are owner-controlled. Copy the selected area's `runtime/playtest.route` into its playtest project only when installing the matching runtime.

- **City:** 213 points / 5 visible milestones, 852.5m market-and-countryside return. Start at the tested market spawn; follow Market stalls → West town edge → Meadow trail → Woodland bend → Return to town. This is an exploration route, with fixed source UCX collision and no relocated combat/vendor quest. Full points and native CPU collision evidence are in `city/route-points.json` and `city/readiness.json`.
- **Jungle:** 28 points / 5 visible milestones, 1,031.04m closed loop. Follow Hidden Pool bank → Fallen Shrine → Canopy Watch → Eastern trail → Back at jungle entry. Walk on the paths; the wildlife is ambient. This runtime route starts on the first measured trail point rather than the older review camera's approximately 10m entry connector. The existing watch quest remains a separate option described in `jungle/ROUTE.md`.
- **Grandstone:** separate owner-reviewed sectors are provided by its support cooker. An explicit transition between sectors is not a claim of one continuous gate-to-hall walking route.

Each route begins with `VALWOOD_PLAYTEST 1`, followed by `name "..."`, `spawn x eyeY z yawRadians pitchRadians`, `count N`, and exactly N `path "label" x groundY z radius [checkpoint]` lines. The optional literal `checkpoint` marks a visible progress milestone; other points keep the player on the tested corridor. Heights are supporting feet heights, not eye heights. Reaching a node requires its horizontal radius and feet within ±2m of the recorded floor. Progress is sequential, including on the return segment; proximity to a later overlapping point cannot skip the route.

The codec accepts at most 256 nodes / 64KiB, bounds finite coordinates and angles, and rejects extra, missing or malformed records atomically. `State` owns its route copy; ordinary `update`/`reset` has no filesystem or allocation work. It tracks progress only. The viewer must use its normal movement, collision and destination-loading code.

Saved sessions use a separate `VALWOOD_PLAYTEST_SESSION 1` codec with section, canonical route fingerprint, next index, camera and first/third-person mode. These sessions must be stored separately from `presentation.progress`. Stale route fingerprints, out-of-range indices, malformed data and nonfinite/out-of-world camera values are rejected. For a section change, stage a new State and camera before committing them. The 64-bit fingerprint detects revision mismatch; it is not an authentication/security digest. Root's viewer integration owns atomic writes, F5/F8/reset UI and native verification.

CPU regression command, from `new_engine`:

```sh
PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover \
  -s tests -p world_playtest_slice_test.py -v
```

All three tests passed on 2 November with C++17 / `-Wall -Wextra -Werror`: strict codec and session rejection/state restoration, plus load/roundtrip/ordered completion of the actual city and jungle routes. City heights came from the current primary-priority/countryside fallback CPU preflight. Jungle heights came from the actual atlas walking function plus current Studio sculpt and baked surface offset through a standalone CPU helper. `runtime-route-provenance.json` records source/route fingerprints. These tests do not render, simulate an entire walk, or prove the integration's inputs, UI and performance; those require the scheduled native playtest.
