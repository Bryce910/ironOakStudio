# Offline opaque color texture blocks

The engine can load precomputed BC7 blocks for fully opaque sRGB **base color and emissive color only**. Source GLBs and embedded image bytes stay unchanged. Resolution, full mip count, sampler filters, wrapping and anisotropy remain unchanged. This is lossy color compression; it does not claim byte-identical sampled RGB. Normal, ARM, facade auxiliary/data maps, MASK/BLEND materials and nonopaque source images retain their existing exact RGBA upload.

The separate offline cooker uses pinned MIT-licensed `bc7enc16`, maximum quality parameters and clamped 4×4 edge padding. It obtains each reference mip from the original sRGB RGBA8 upload, then encodes every mip independently. GPU decoding must pass all of these guards before a cache is written:

- Base RGB PSNR at least 38 dB (`RMSE <= 3.2102598` in 0–255 units).
- Every mip RGB RMSE below 10, and each channel's absolute mean bias below 3.
- Every decoded alpha byte exactly 255. Mode 6 alpha endpoints and shared P bits are finalized to guarantee opaque alpha; the format parser independently checks these bits.

Any failed image keeps its entire original chain. `projects/valwood_world/texture_blocks/manifest.json` records each admitted/rejected image and every admitted mip's measurements. The game never links or runs the encoder. `make cook-world-texture-blocks` explicitly creates the cache; it is not part of a game build. Never cook during a timed FPS run.

The runtime reads a bounded versioned file containing its full original encoded image. The filename fingerprint is a lookup hint; full source byte comparison is authoritative. CRC64 covers descriptors, source and blocks. The parser requires the exact chain down to 1×1, dimensions/pixel/file/source budgets, strict quality certificates and the opaque mode 1/6 block recipe. The runtime additionally compares cached dimensions to `stbi_info` of the original image, rejecting an internally valid resized cache. A missing, stale, corrupt, unsupported or rejected entry falls back to the original full-resolution upload. Upload-time GL queries verify actual compressed format and storage. Drawing performs no cache I/O, compression or readback.

`VALWOOD_QA_DISABLE_OPAQUE_BC7=1` selects the original upload path at model creation for A/B diagnostics; no saved graphics preference changes. A policy bit prevents compressed and original entries from being confused by the existing shared texture cache. `world_model_texture_block_stats()` exposes loading-only candidate/success/miss/rejection/failure counts and actual compressed versus reference byte totals for native telemetry.

Validation:

- `make test-world-texture-blocks-format`: CPU corruption, bounds, source identity, full-chain, mode/alpha and atomic-write checks.
- `make test-world-texture-blocks-gl`: actual village/forest full-resolution mip decode against original references, strict color/alpha guards, actual storage, unpack/PBO restoration, valid-checksum resized-cache rejection and corrupt/missing fallbacks. Default uses isolated software GL; native runs are scheduled centrally.
- Native driver encoder diagnostics and the accepted offline CPU encoder probe are retained under `docs/performance/demo-finish/compression-diagnostics/`. Earlier Mesa on-upload encoder experiments failed the color guard and were rejected. Their results do not describe this offline encoder.

Reduced texture bytes must be confirmed in the engine's own native allocation telemetry. A timed complete route and all viewpoints must pass afterward; a lower memory footprint alone is not proof of 45–60 FPS or finished visual quality.
