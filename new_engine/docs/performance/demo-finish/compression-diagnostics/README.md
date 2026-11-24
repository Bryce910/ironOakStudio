# Native BC7 diagnostics

These isolated capability/quality probes ran on the AMD Radeon RX 5300 with
Mesa 25.2.8. They do not measure game frame rate. No model, source PNG, shader,
runtime loader or graphics preference was changed by these historical driver-encoder probes.
The proposed on-upload Mesa encoder was rejected. The later offline CPU encoder uses a different path, documented below.

The driver supports `GL_ARB_texture_compression_bptc` and accepts uncompressed
RGBA input with `GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM`. Queries confirmed actual
BC7 storage at every mip: a 1024-square chain occupies 1,398,128 bytes versus
5,592,404 bytes for RGBA8. Warm upload and mip-generation times are recorded in
the logs; they are loading costs, not frame-time measurements.

Quality prevents enabling this path. The prior strict contract remains unchanged
at **every mip**: RGB RMSE below 10 byte values, absolute signed mean error below
3 byte values in each channel, and fully opaque alpha retained exactly. Any
failure would require retaining the entire original RGBA8 chain. No threshold
was relaxed and no mip was excluded.

Representative embedded sources include the village house A wood, plaster,
roof and stone maps, pine bark, and masked pine/oak/fern vegetation. The masked
images were excluded from compression. Every tested opaque BC7 mip retained
alpha 255 exactly. The encoded source files were opened read-only; their sizes
and modification times remained unchanged.

| Experiment | Opaque sample cases | Cases failing the strict full-chain contract |
| --- | ---: | ---: |
| Independent original RGBA reference mips | 7 | 7 |
| Same inputs with `GL_NICEST` compression hint | 7 | 7 |
| At most two measured bias-correction retries per failed mip | 7 | 5 |
| Edge-pad each mip to complete 4×4 encoder blocks | 7 | 7 |
| Edge-padded blocks plus at most two correction retries | 7 | 5 |

Two successful retry cases refer to the same pine bark image, so they represent
one unique image. The retry experiment changed temporary encoder input copies;
quality was always measured against the unchanged original RGBA reference. It
is not a production feature. Most remaining failures occur at 1×1 or 2×2 mips,
where the driver output jumps between quantized colors. One pine trunk map also
had a coherent base-level blue bias beyond the existing limit; feedback reduced
that bias, but its final tiny mip still failed.

The edge-padding probe clamped the last row/column into complete 4×4 blocks,
compressed those scratch images, read back their BC7 bytes, and installed the
blocks with the original mip dimensions using `glCompressedTexImage2D`. Actual
storage queries and decoded dimensions remained correct, and alpha stayed 255.
However, 1×1 errors remained identical to the unpadded path. This rejects the
hypothesis that zero-filled encoder padding alone caused the tiny-mip failures.
The padded source and both native logs are retained alongside the earlier probes.

The raw driver encoder is visibly lossy in color even when opaque alpha is exact:
base-level PSNR was 31.92 dB for the roof, 35.34 dB for wood, 37.42 dB for plaster,
and 34.64 dB for the pine trunk map. These pixel metrics do not establish that
the in-game appearance meets the user's visual target. In-game comparisons and
a per-texture quality gate would be required before any future integration.

The synthetic masked probe also shows why compression must not be applied
generically: it changed 30,071 base-level alpha-threshold classifications, and
generating later mips from an already compressed level accumulated further
error. Linear normal/ARM maps and masked foliage remain exact in production.

The three diagnostic source files and all logs are retained in this directory.
`actual-embedded-quality-probe.cpp` supports `VALWOOD_BC7_NICEST_PROBE=1` and
`VALWOOD_BC7_FEEDBACK_PROBE=1`; the latter permits at most two retries per mip.
Native runs require the central renderer slot to be idle. No further native
experiment should run concurrently with game acceptance profiles.


## Offline CPU encoder follow-up

`offline-cpu-quality-probe.cpp` and `offline-cpu-quality-native.log` test pinned `bc7enc16` at high quality on the same seven representative opaque image cases. Every full mip chain passed the original RMSE/bias guard with exact alpha255. Base PSNR was approximately 46–48 dB on village roof, stone, wood/plaster and forest bark. This result supports testing a precomputed cache; it is not a game FPS result.

`offline-cooker-native.log` records the first complete project cook; `offline-cooker-final-native.log` records the final base/emissive-only inventory (69 accepted, four originals retained), with per-image fallback whenever any mip misses the strict guard. The runtime only admits opaque base/emissive color and does not invoke the CPU encoder. See [the cache contract](../../../TEXTURE_BLOCKS.md) for the parser, quality and fallback rules.

The final inventory contains 653,350,140 reference RGBA mip bytes and 163,339,408 BC7 bytes, a 490,010,732-byte reduction for the unique eligible image set. `../texture-blocks-native-quality.log` verifies the actual runtime loader on six unique images/71 full mips, including malformed-cache fallback and GL unpack-state restoration. These are texture-storage and quality checks, not an FPS result.
