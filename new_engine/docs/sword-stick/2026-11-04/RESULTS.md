# Directional sword controls — November 4, 2026

The V4 input checks below did not catch a visual guard defect subsequently reported by the player: selected left/right directions used high recoil poses, and captures preceded settled transitions. The follow-up [held-guard correction](/home/computer/Desktop/final_engine/new_engine/docs/sword-guard/2026-11-04/RESULTS.md) replaces those poses and checks the renderer's actual settled guard in both camera views. Treat V4 as historical input verification, not final visual acceptance.

The local city, jungle and Grandstone playtest launchers now use `build/sword-stick/v4/bin/rpg`. Combat right-stick gestures initiate left/right cuts, an upward rising cut or a downward overhead cut without RT. Center the stick between swings. Holding LT changes the stick to left/right/high/low guarding, with centered LT guarding thrusts. LT takes priority over attack buttons. Releasing LT, holding a direction, countdowns and pause/resume cannot replay a consumed gesture.

Outside combat the right stick continues to control the camera. Existing keyboard/mouse controls, RT/X alternate attacks, RB thrust, B dodge and camera switching remain available. Changing guard direction alone does not refresh the parry window; a fresh LT press is required.

## Verification

- [Native rendered regression](native-v4/results.json): 38 checks, 112 real SDL virtual-controller inputs, 10 captures, all passed on the RX 5300. Includes all four stick-only attacks, five LT guards, alternate-button guard priority, countdown, held-stick suppression, pause/resume and first/third-person behavior.
- Each attack capture is from the actual strike phase; each guard capture records the rendered guard direction. Per-image state sidecars are validated by the runner. No combat-state, player-pose or story-state assignments drive the test.
- [Input policy tests](cpu-policy.log): 1,132 checks, including natural combat block/parry outcomes. [Existing controller tests](cpu-controller.log): 57 checks plus UI navigation. [Evidence-gate tests](cpu-evidence-gate.log): 9 passed.
- [Native and Windows build review](/home/computer/Desktop/final_engine/new_engine/build/sword-stick/v4/READY.json): both compiled; Windows DLL closure passed. Previous binaries, source projects and archives preserved. Windows packaging and physical Windows/controller testing were not performed for this change.

The rendered regression used an isolated city project. Source saves/settings and the executable were unchanged by the run. This verifies input behavior and rendered animation through SDL; physical controller feel and sustained frame pacing still need hands-on review. The jungle and Grandstone use the same controller code, but were not replayed in this test.

## Local activation

The shared playtest launcher selects the accepted V4 binary; the previous binary remains in `build/playtest-slices/bin/rpg`. The prior launcher and controls guide are saved under [before](before/activation-inputs.json). The apartment/opening launcher, its runtime, and its progress are unchanged. The November 2 Windows playtest ZIPs are historical and do not contain these new controls.

Native SHA256: `d3c17dc8cc2456d956874f7792fd90dad345be158320c2f56c97be2860a34d07`.

Windows SHA256: `4960e3b478939a5ee262d2b47b68a65bef493ee9e1bba9d78ae3508a314ad94e`.
