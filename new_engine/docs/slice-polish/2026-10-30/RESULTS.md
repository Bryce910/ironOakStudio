# Valwood presentation polish — 30 October 2026

This pass prepares the existing playable slice for showing on this Linux RX 5300 computer. The final candidate is `builds/relocated/Valwood Presentation Polish 2026-10-30 r2`. The original presentation package is preserved. Final route measurements and package verification are recorded in readiness.json.

## Delivered changes

- Contact-triggered hit/parry/guard sparks, ground impact/dodge dust, and weapon ribbons. Events follow actual combat results. Blade landmarks are measured from the existing cooked weapon geometry and interpolate with the rendered poses. First-person ribbons follow the viewmodel projection. Effects use fixed pools (192 particles, 40 trail segments), one instanced pass, opaque-depth occlusion and temporal reactive output. The shader is warmed during loading.
- Tighter striking distance, short contact holds, directional body alignment, held defeat reaction, and swing/contact audio tied to gameplay events. Dedicated audio channels preserve parry/impact tails. Reduced camera effects disables contact camera impulses and holds.
- Closer, collision-checked and eased dialogue framing, quieter head/neck attention, wrist/chest gestures and phrase timing. The user selected existing assets and text dialogue for today's showing. Facial expression, lip/finger animation, distinctive hero art and acted dialogue remain outside that selected content scope. Optional human WAV playback and an exact four-line recording script are prepared; no voices were fabricated.
- Existing guided route, chapter transitions, quest markers, pause/journal UI, scenery dressing, recognition, one-time Watch reward and ending retained and rehearsed.
- Presentation-only CPU/GPU overlap retains VSync and the existing quality settings. A paired 3,600-frame castle segment improved from 59.73 average / 48.77 1% low FPS to 59.95 / 59.30, with sub-45 frames changing from one to zero. Ordinary world/editor completion policy is retained.
- Enter retries a defeated encounter. Native rehearsal found and fixed a same-frame retry race: pending defeat is now delivered before reopening the encounter, preventing the quest host from keeping the old encounter active.

## Validation already completed

209 presentation checks pass, including immediate retry event ordering, combat rules, rewards and save/resume. Existing motion and live animation checks pass. Recorded dialogue tests verify missing/corrupt fallback, preload, one playback per line, pause/focus, volume, stop and channel ownership. The native VFX test verifies 480 changed reactive spark/dust/trail pixels, depth occlusion, feedback rejection and retained caller textures. Package contracts pass.

The final packaged native journey passes actual defeat, Enter retry, the retry dialogue choice, all three trial rounds, the crossing encounter, Rowan's report, Corvin's recognition, reward, save/restore and ending. It uses 712 input events and 377 collision-tested walking steps with zero blocked steps; GL errors and remaining texture references are zero. Source authoring is unchanged. Combat telemetry records 15 hits, ten parries, six blocks, 14 incoming hits (including deliberate defeat), 16 dust bursts and 670 trail segments, with zero effect failures. Peak effects are 47 particles and 11 trail segments.

A 4,367-frame continuous trial span averages 59.95 FPS, with a 58.90 FPS 1% low and one 22.77-ms frame. The 292-frame crossing span averages 59.95 FPS with a 59.07 FPS 1% low and a 16.97-ms worst frame. These spans start after 120 fighting frames and include round/retry countdowns; chapter loading is outside them. Full mixed-journey loading intervals are retained in the raw profile and are not represented as sustained gameplay performance.

Native final-package first-person impact and third-person parry captures pass and were visually inspected. Source close-up framing was inspected in native-conversation/preview.png. Full packaged route measurements follow separately; no compilation, asset scans or video encoding runs during timed route intervals.

## Evidence

- `cpu-immediate-retry.log`, `cpu-final.log`, `cpu-visual-finish.log`, `gpu-vfx-final.log`, `package-contracts.log`.
- `completion-comparison/`: paired native pacing experiment at identical quality.
- `release-r2-journey/`: final package's real defeat/retry and complete journey.
- `combat-performance.json`: sustained encounter spans derived from the raw CSV.
- `release-r2-impact-first/`, `release-r2-parry-third/`: final package combat captures.
- `release-r2-routes/`: all four complete fixed-quality walking routes.
- `frozen-source.json`, `package-build-r2.json`, `readiness.json`: final source identity, package creation and consolidated release checks.

The earlier `native-journey-first`, `native-complete-retry` and `release-journey` directories retain failed/aborted candidates: missing private-project shader integration, an incomplete retry driver, and the real immediate-retry race respectively. These are superseded by the passing r2 journey, not discarded evidence.

This is a same-machine presentation slice. It is not a completed AAA production or a clean-machine/cross-platform certification. Strict every-frame 45-FPS results are reported independently of statistical performance. Controller hardware remains unverified. Cold chapter preparation can pause behind the transition screen.

## Final packaged route and system results

| Camera / route | Average FPS | 1% low FPS | Worst ms | Sub-45 frames |
|---|---:|---:|---:|---:|
| third / rainforest | 57.08 | 45.66 | 24.19 | 13 / 7580 |
| third / castle-west-street | 59.95 | 59.49 | 18.44 | 0 / 10380 |
| first / rainforest | 57.52 | 46.42 | 23.31 | 9 / 7580 |
| first / castle-west-street | 59.95 | 59.61 | 17.70 | 0 / 10380 |

All four statistical targets and route correctness checks pass at identical quality. Both castle routes meet the strict every-frame 45-FPS floor. The forest retains 22 brief dips, so the overall strict floor is unmet. Across the four routes, the earlier package had 36 sub-45 frames and a 29.39-ms worst frame; this package has 22 and a 24.19-ms worst frame.

Native desktop audio starts through PulseAudio with zero update failures; system preflight passes. All 1,490 package files verify, all 484 frozen source inputs match, and the authored scenes, graphics preferences and editor recovery/workspace files match the pre-task backup. The original package binary is unchanged. The rejected intermediate package was cleaned after retaining its manifest and failed test evidence. The standard launcher now selects r2.
