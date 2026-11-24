Native promotion is prepared, not executed. Root owns GPU acceptance and the
separate exact final-plan GO. Original prepared plans v1–v4 remain preserved.

The current prepared plan is `native-promotion-v5/PLAN.json`, SHA256
`5dd80284070283d687d346793efff33685dbd61f1be20d24cdda2b57bd909036`.
It has no conflicts: 24 small shader replacements/additions, eight launcher
bindings, and a same-filesystem rename of the owned selected Opening copy.
Original native C21 settings, layout, opening controls and small saves are
staged for the new project; their original project is never modified. All
139 Opening shader files come from the frozen tested candidate inventory.

The four existing World shader symlinks are replaced as project entries;
their canonical shader targets are unchanged. Two required includes are new.
Per-area City, Jungle and Grandstone variants are retained. Each project's
water shader also receives the previously accepted Landscape prerequisite.

After all QA has finished, root writes a receipt using this minimal schema.
Each evidence entry is a passed RESULTS.json and its exact SHA256; candidate
results must match their sibling PLAN.json, the v2 binary, and one of the four
complete frozen shader inventories. Include candidate native and Windows
evidence. Baseline results may also be included for comparison.

```json
{
  "schema": "valwood.render-quality.native-promotion-acceptance.v1",
  "passed": true,
  "reviewer": "root",
  "native_binary_sha256": "58fb2efff01d044fd6f63a82d3b68c020e7719b19b63ab873c2c6966e517998a",
  "windows_binary_sha256": "a1afbc97af0a189932956ecab5c1e60fc14a95f606f98359c05141ac31cb836c",
  "runtime_plan_sha256": "91a45d524c2ef84b980b56b3688fed38435bc8ecebeb7c10e053fa2119c8611a",
  "accepted_checks": {
    "native_visual": true,
    "actual_first_journey_native": true,
    "actual_first_journey_windows": true,
    "windows_four_area_smoke": true,
    "timing": true,
    "motion": true
  },
  "evidence": [
    {"path": "/absolute/path/under/docs/render-quality/2026-11-06/job/RESULTS.json", "sha256": "exact-result-sha256"}
  ]
}
```

The acceptance is root's explicit visual/functionality/performance decision;
the tool verifies its integrity and exact tested runtime/shader identities.
Motion evidence may use the untimed BMP/world sidecar harness; the acceptance
validator does not require profile mode or a particular sidecar format.

Run from `/home/computer/Desktop/final_engine/new_engine`, with the receipt
saved under this documentation lane:

```bash
python3 docs/render-quality/2026-11-06/native_promotion.py finalize \
  --plan docs/render-quality/2026-11-06/native-promotion-v5/PLAN.json \
  --plan-sha256 5dd80284070283d687d346793efff33685dbd61f1be20d24cdda2b57bd909036 \
  --acceptance docs/render-quality/2026-11-06/FINAL_GPU_ACCEPTANCE.json
```

Finalize holds the shared game lock and creates only a private `final/`
folder. It snapshots the complete small QA control/opening/shader/save entry
set and records immutable asset metadata; asset banks are not hashed or
copied. It stages desired restored controls and candidate Opening shaders,
binds provenance to final root acceptance, and prints FINAL_PLAN.json's SHA.
Finalize only after every root GPU job is complete. A failed lock acquisition
stops without modifying the QA project or public paths.

Root reviews that exact FINAL_PLAN SHA and issues the separate GO. Then:

```bash
python3 docs/render-quality/2026-11-06/native_promotion.py apply \
  --plan docs/render-quality/2026-11-06/native-promotion-v5/final/FINAL_PLAN.json \
  --plan-sha256 EXACT_FINAL_PLAN_SHA
```

Apply verifies all current before-identities, pinned staged/rollback bytes,
original C21 entry sets, compiled runtime receipts, both actual binaries,
acceptance evidence and ordinary destination parents under the shared lock.
It renames the owned QA project, restores original native controls/saves,
installs frozen shaders and its own origin, then updates public launchers last.
Every file replacement is atomic; the full operation is not a filesystem
transaction. A pinned journal supports guarded rollback of the changed
subset if an ordinary exception occurs. A crash between a replacement and
journal persistence can require manual review; guards refuse unknown state.

Apply writes APPLIED.json only after CPU postverification succeeds. Public
launcher smoke is a separate root GPU step. Keep rollback until that passes.
The same exact final plan arguments work with `verify` and `rollback` instead
of `apply`. Rollback checks the approved journal and every current after-state
before changing anything; it refuses independently changed project/save or
public files so later user work cannot be silently discarded. Restored files
are checked by bytes, mode and link target because atomic restoration changes
inodes. Newly owned origin/parent are removed only when still matching and
empty. No command performs GPU work.

The old arena camera wrappers/runtime/origin/project, R11 runtime, original
C21/demo/source assets, legacy binaries, sealed worlddesigner projects/ZIPs,
and independent playtest projects remain outside the mutation set. The frozen
runtime builder deliberately recorded pre-promotion public wrapper hashes;
the promotion receipt records the authorized exceptions. Do not claim the
old builder's full protected-wrapper validation still passes after promotion.

The 47-file future canonical patch is reference-only: 30 files are unchanged
accepted v7 Landscape integration missing from canonical, and 17 are render
changes or the merged viewer/scenery. Accepted whole-file arena camera code
already equals canonical and is absent. Its source-hashed future recipe
requires separate review/build/QA and preserves per-area shader variants.
This promotion does not apply that patch or rebuild stale canonical objects.

`NATIVE_PROMOTION_GUARD_CHECK.json` records a tiny disposable CPU fixture:
unapproved journals, edited after-state and dangling rename sources reject
before mutations; ordinary bytes/modes, World-style symlinks, QA rename,
owned origin and empty parent restore successfully. No public paths or GPU
were used. The focused independent review's guard findings were addressed.

Finalize-v4 safely rejected before writes because its root entry guard counted
C21 asset directory links as control files. Prepared v5 keeps all 14 original
controls guarded and separately pins the nine resolved directory links, without
traversing their asset targets. Unexpected dangling/file links remain rejected.
`NATIVE_PROMOTION_DIRECTORY_LINK_GUARD_CHECK.json` records the tiny regression
fixture. V5 prepared-state and final GPU acceptance validation both pass; the
accepted binary/shader/source plans are unchanged.
