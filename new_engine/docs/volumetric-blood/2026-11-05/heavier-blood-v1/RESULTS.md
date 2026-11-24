# Heavier, shorter-lived combat blood — accepted V4

The large purchased fluid animations now play three times faster, with per-variant lifetimes of approximately 0.65, 0.82 and 0.92 seconds. The delayed second lobe brings the maximum airborne lifetime to 0.953 seconds. Each mesh retains its actual contact floor; fragments reaching that floor are clipped instead of hanging below it. Mesh slots expire with their individual source clips.

Procedural droplets and clots use 9.81 m/s² gravity, lower upward launch speeds, darker pigment and ground-contact removal. Only two small mist puffs remain per hit, lasting at most 0.12 seconds. Droplets/clots last no more than 0.62 seconds. Impact particle counts, combat timing, damage, weapons and directional controls retain their existing gameplay behavior.

The fluid and floor stains have a darker crimson pigment, restrained incident-light highlights and ground texture variation. The persistent 128-mark ground budget and world anchoring remain intact; restart or leaving the arena clears the marks. No severed parts or ragdolls are included in this pass.

Full native RX 5300 acceptance passed **34 gameplay checks and 20 actual captures**, including ordinary defeat/retry, physical arrows, body versus steel impacts, pause, all eight wins and restart. At 1.05 seconds after finishing damage, both the renderer and CPU stage had zero airborne fluid; five persistent ground marks remained visible at full opacity. The original ground witness also survived later opponents beyond 30 seconds with unchanged placement. No stains were replaced during the full run.

The bounded timing log averaged **59.98 FPS**, with a 59.08 FPS one-percent low and 0 frames below 45 FPS. This arena workflow includes loading and captures; it is not a sustained whole-game benchmark. Render, streaming and texture-reference failures were zero.

CPU lifecycle tests passed 34,826 checks in optimized and address/undefined-sanitizer builds. Native GPU checks verified early visible fluid, darker shading, actual-floor clipping, source expiration, unchanged persistent-stain pixels after 32 seconds, pause, depth ownership and fixed resource budgets. Separate procedural VFX CPU/native-GPU checks passed. The GPU floor witness contained zero blood pixels below the captured floor, compared with 73 in the deliberately lowered-floor control.

Linux and Windows builds are ready. The local Linux arena is the activation target; older Windows transport ZIPs and other area launchers retain their existing versions. Original purchased assets, previous banks, V1–V3 binaries, prior proofs and saves are preserved.

- [Full gameplay evidence](/home/computer/Desktop/final_engine/new_engine/docs/volumetric-blood/2026-11-05/heavier-blood-v1/native-v1/results.json)
- [Settled fluid with ground blood remaining](/home/computer/Desktop/final_engine/new_engine/docs/volumetric-blood/2026-11-05/heavier-blood-v1/native-v1/captures/blood-settled.png)
- [Actual settling metadata](/home/computer/Desktop/final_engine/new_engine/docs/volumetric-blood/2026-11-05/heavier-blood-v1/native-v1/captures/blood-settled.json)
- [Native source-fluid GPU evidence](/home/computer/Desktop/final_engine/new_engine/docs/volumetric-blood/2026-11-05/heavier-blood-v1/gl-v1/result.json)
- [CPU evidence](/home/computer/Desktop/final_engine/new_engine/docs/volumetric-blood/2026-11-05/heavier-blood-v1/cpu-v1/result.json)
- [Both-platform build proof](/home/computer/Desktop/final_engine/new_engine/build/volumetric-blood/runtime-v4/READY.json)

A new silent 5.32-second clip contains 319 consecutive native gameplay frames at 60 FPS, including the initial impact, finishing damage and settled ground aftermath. The short recording uses an intentional bounded partial-QA exit; full 34-check acceptance is the separate run above. The movie and frame hashes were verified, source/bank/binary inputs remained unchanged, and only the new temporary raw frames were removed after export.

- [Actual gameplay clip](/home/computer/Desktop/final_engine/new_engine/docs/volumetric-blood/2026-11-05/heavier-blood-v1/video-v1/heavier-blood-combat.mp4)
- [Clip provenance](/home/computer/Desktop/final_engine/new_engine/docs/volumetric-blood/2026-11-05/heavier-blood-v1/video-v1/results.json)
- [Local activation](/home/computer/Desktop/final_engine/new_engine/docs/volumetric-blood/2026-11-05/heavier-blood-v1/ACTIVATION.json)
