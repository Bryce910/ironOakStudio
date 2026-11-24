# Opening review

The canonical opening is a run-down modern house, cinematic transport, then the Meadows of Valois. Grandstone and the existing sword-combat route remain separate previews. This chapter ends after examining the waystone, with free exploration still enabled.

The modern house is a new composition using owned furniture and existing CC0 textures. It has exact collision, a supported walk route, practical lights, floor trim, a notebook interaction and a front-door trigger. Furniture metallic/roughness textures are repacked for the native shader's ARM convention: unused glTF red is not ambient occlusion. Original source assets are unchanged.

The opening state module owns bounded cinematic time, pause, hold-to-skip with release between shots, spatial objectives, and strict versioned checkpoints. The runtime keeps the player's supported capsule separate from the cinematic render camera. Both establishing rails finish at the exact playable pose and FOV. Transport changes destination once under black, then freezes the reveal while the destination prepares.

Keyboard and SDL controller inputs share the same interaction/menu commands. Mouse look is captured for play; pause, focus loss and cinematics release it. Walking/look remain disarmed until sticks and movement keys return to neutral. Jump is a rising-edge action. First person is intentional while the modern father asset remains deferred. Sensitivity, deadzone, inversion, reduced motion, music and vibration controls are available in pause; graphics use the existing settings panel.

The home is acoustically quiet, uses wood footsteps and does not play tavern ambience. The meadow retains the existing outdoor ambience. No new acted dialogue, facial animation, father rig, armor transformation or weapon progression is claimed.

CPU checks cover 5,430 state/save/camera cases, production model loading, 322 actual house-route steps and stable floor resamples, blocked walls/door/furniture, private-state isolation and SDL mixer behavior. The native runner walks the actual house and approximately 92 m meadow route using SDL input, without assigning the player or story state. It captures five stages and checks title, cinematic locks, held input, controller look, keyboard/controller pause confirmation, jump, interactions, checkpoint restore and retry.

The final `FINAL_REVIEW.json` binds accepted runtime hashes, scene hashes, native and Wine evidence, source-state guards, captures, package verification and limitations. Native Windows hardware and a physical controller remain outside this machine's validation scope. Whole-run FPS includes streaming, explicit load/retry and capture costs; it is not a strict frame-floor certification.
