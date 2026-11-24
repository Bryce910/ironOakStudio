# Early tavern pass validation

The optimization renders the same opaque tavern once, earlier, only while the
camera is inside its conservative local prism. Shadows and reflections retain
their existing draws. It does not change saved85% resolution or lighting quality.

`check_castle_streets.py --native` now requires `early_opaque` telemetry for new
normal cases: alias `village_tavern`, enabled, equal early/late-exclusion counters;
all1800interior frames, some full-route frames, zero outside street/market frames.
Historical `--verify-existing` reports remain readable without inventing missing
telemetry. New performance runs use fresh output directories and retain every
measured interval after the declared120frame warmup.

Untimed appearance A/B, run only after the final build and source freeze:

```sh
python3 tools/check_castle_early_pixels.py --native \
  --views castle-tavern-interior castle-tavern --frames 180 \
  --output docs/performance/castle-streets/early-pixels
```

Each view launches two sequential ordinary native captures: a reference with
`VALWOOD_QA_DISABLE_EARLY_OPAQUE=1`, then the optimized pass. Both use the same
saved Custom85% manual Balanced effects,1600×900drawable, no HUD, dusk and fixed
visual time22. The helper sanitizes inherited QA/software/disable environment
flags, uses the normal launcher lock, and hashes frozen inputs only while idle
before and after the batch. It never builds, cooks or edits preferences.

Full-size native metadata, all resource/error counters, both camera/lighting
states and mode-specific early/late counters must match their contracts. The
RGBA image comparison requires exact equality: even one channel's one-LSB
difference fails. It writes a true-magnitude difference PNG and numerical report;
there is no tolerance or automatic acceptance. Early opaque scene capture may
alter refraction near a visible water edge; such a difference requires review.
These180frame captures make no FPS claim.

Read existing paired results without launching or hashing runtime models:

```sh
python3 tools/check_castle_early_pixels.py --verify-existing \
  --views castle-tavern-interior castle-tavern \
  --output docs/performance/castle-streets/early-pixels
```

Compare two captures produced manually, with no native-report validation:

```sh
python3 tools/check_castle_early_pixels.py \
  --reference /tmp/reference.bmp --candidate /tmp/optimized.bmp \
  --report /tmp/early-pixel-comparison.json
```

Timed performance is measured separately with the normal native benchmark helper
and the user's unchanged saved profile. Compare total GPU and complete frame
intervals as well as stages; the early pass time is included in the forest total.
