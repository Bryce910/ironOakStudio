# Walking demo sound and motion review

`engine/world_demo_audio.cpp` adds environmental audio to the atlas walking viewer. The original viewer never initialized or updated the legacy audio engine, and this project had no sound-effects folder. The new module owns its optional audio device and finished PCM resources without changing the legacy scene audio code.

Eight quiet beds blend wind, woodland, creek, fire, rain, market activity, tavern furnishings and night insects. Market and tavern beds contain timber, cloth and crockery; they are **not recorded crowds or speech**. The forest birds and other textures are original procedural sound design. `tools/build_demo_audio.py` creates all 20 shipped clips offline, with deterministic seeds and per-file SHA256 provenance in `projects/valwood_world/audio/demo/manifest.json`. No third-party recordings were downloaded.

Creek and pool audio follow the actual authored water boundary, and fire audio receives the renderer's actual flame origins. Wind, birds and rain become quieter under cover and indoors. Leaving window focus immediately mutes the owned voices and clears pending footstep distance; regaining focus fades the mix in. Spatial sources pan smoothly relative to camera yaw. A 0.65 s exponential crossfade gives the same response at different frame rates.

The player has three footfall variations for soil, paving, timber and wet soil. Cadence follows actual horizontal translation on the ground; blocked movement, looking around, flight and large camera cuts cannot produce steps. Walk and sprint use 1.20 m and 1.85 m spacing, respectively. Stalled frames never replay missed steps as a sound burst.

Runtime storage is bounded: eight persistent bed voices, four centered transient voices, at most 32 copied fire emitters, and 23,221,312 decoded PCM bytes at 44.1 kHz stereo. Every clip loads before gameplay. Update contains no file access or C++ heap allocation. Persistent loop panning effects initialize once; transient footsteps deliberately avoid effects that SDL_mixer would otherwise allocate for each finished clip. At maximum possible bed and footstep signal peaks, the module's conservative linear sum is 0.848825 full scale.

## Validation

- `dummy-backend-tests.log`: all 20 clips load; 35 steps over 42 m at 30/60/144 FPS; material and zone behavior; roof/rain ducking; focus; invalid input; no C++ hot allocations; bounded 12 voices; partial-failure cleanup; borrowed device/channel ownership.
- `validation.json`: SHA256/source metadata, signal peaks/RMS, loop seam versus actual adjacent-sample variation, PCM budget and conservative mix headroom.
- `population-cpu-tests.log`: existing 140 residents / 80 poses / 95 blend batches preserved. 600 s at 60 Hz includes 8,564 exact-pose handovers, 1,122 opposite-foot stops, maximum 0.0541 rad/frame turn and 4.152 m/s² acceleration. 350,280,000 crowd pairs and 5,040,000 actual collider/ground probes retain at least 1.400001 m separation. The original 128 actors' regression hash is unchanged.
- `pets-cpu-tests.log`: existing 12 cats/dogs retain 432,000 actual-scene probes, at least 2.4 m pair separation and maximum 0.0290 rad/frame turn, with no phase snaps or hot allocations.

The motion review found no discontinuity in the existing distance-driven strides, eased acceleration/deceleration, pose interpolation or dwell turns. Those systems and their model poses were preserved. Higher quality turns and facial performance still require dedicated animation source clips; these CPU checks do not constitute a visual review of every animation.

The dummy backend verifies execution and signal/resource contracts, not the quality of speakers or headphones. LeakSanitizer cannot run in the ptrace-controlled sandbox; address/undefined sanitizers run with leak detection disabled, and explicit device/channel/clip cleanup is covered by the fixture. The integrated viewer must record its actual driver and a real-device listening review remains useful. Remove the benchmark's `SDL_AUDIODRIVER=dummy` override for the final interactive demo.

## Integration

Add `engine/world_demo_audio.cpp` to the engine's C++ source list. Include its header, create the optional module with the project-relative `audio/demo` directory, copy the existing flame-emitter origins into `WorldDemoAudioFire`, and update once per frame after the current weather intensity is computed. Supply camera coordinates/yaw, real frame delta, grounded/sprint state, actual focus/minimize/loading state, current hour/wind/rain and the existing actual tavern/roof tests. Free the module before SDL shutdown. The module must not be paired with the legacy `audio_update` loop in the atlas viewer.
