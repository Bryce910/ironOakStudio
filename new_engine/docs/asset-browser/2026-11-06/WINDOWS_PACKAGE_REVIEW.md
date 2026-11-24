Asset-browser Windows package wrapper — root review

build_windows_overlay.py wraps, rather than edits, the exact accepted collision/water cumulative helper (SHA 7aadf72ff5c76f44bfcb06d50aed0933bf27ba1b0d64bf4bf1539d182098667d). It pins the unchanged transaction actions, rollback CLI, dependency parser, protocol test harness, and collision-payload test harness. Original NPC/wind archive parent and 32GB full-delivery identity stay unchanged.

Read-only:
  python3 docs/asset-browser/2026-11-06/build_windows_overlay.py --inspect

Only after root accepts the actual new Windows runtime:
  python3 docs/asset-browser/2026-11-06/build_windows_overlay.py --build --runtime-root EXACT_RUNTIME --runtime-plan-sha256 EXACT_SHA --revision 1

The unchanged helper first produces an immutable private cumulative Collision/Water archive under the asset-browser work root. The wrapper copies its staging tree, adds browser-specific README/update metadata, recomputes authenticated bootstrap rows/index identity, and creates a separately named Asset-Browser candidate. Original helpers, accepted archives, user projects and original candidate archive remain untouched. A new receipt binds every byte, both builders, and the intermediate receipt. No publication occurs.

Run actual inherited protocol and collision gate tests against the final browser candidate:
  python3 docs/asset-browser/2026-11-06/build_windows_overlay.py --test --candidate-receipt EXACT_NEW_RECEIPT --revision 1

The wrapper also exercises four explicit simulated prior-collision-v9 installations using the authenticated v9 runtime identities and the existing exact collision/water bytes. Each fixture must change only installation state, preserve artist scene/model/baseline bytes, remain idempotent, and restore the exact prior state on rollback. These CPU fixtures do not substitute for the normal Windows/Wine editor acceptance.

Public filename, only after root acceptance:
  Valwood-World-Designer-Asset-Browser-Update-Windows-2026-11-06.zip

Rollback README deliberately directs a browser-only rollback to the previous accepted Collision/Water bundle. The inherited previous/ index is the older NPC parent and must not be copied merely to undo browser UI polish in a Studio11-capable installation.
