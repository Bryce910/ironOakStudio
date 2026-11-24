# Independent final evidence review — 2026-11-02

**No unexpected blocker found for the three-project designer orientation.** This review examined existing evidence only; it did not run the game, change projects/assets/shaders, or certify the complete gameplay demo.

## Evidence and failure audit

Reviewed all 14 world JSON sidecars and corresponding logs in `native-editor`, `native-runtime-editors-focus`, `native-orientation`, and `windows-final-orientation`. All sidecars parse. Beyond the runners' renderer/GL/reference gates, the reviewed captures report zero model-streaming failures, critical pending assets, texture-cache rejections/upload failures, main/wildlife material preparation failures, physics errors, gameplay failed ticks, particle failures, stronghold failures, and weather failures. Main material preparation is ready; remaining decoded texture references and GL errors are zero.

The six Linux/Windows orientation logs contain no unexpected model, scenery, material, shader, or collision-loading failure. Each logs 38 missing **legacy optional** sound/music files. The current demo audio bank nevertheless reports ready, with eight beds and twelve footstep samples loaded and zero update failures. Audio uses the dummy driver in these checks, so audible playback remains unverified.

Shader errors and rejected operations in the surface/runtime workflow logs are deliberate rollback tests, not silent startup failures. Surface QA explicitly injects a failed shader, missing image, and unsafe road transaction; it finishes with 3 applies/3 expected rejects and protected imports unchanged. Runtime QA injects its failed shader and finishes its backend phase; the final real UI workflow records 4 applies/4 expected rejects, one actual Graph Compile button acceptance, and successful saved-scene reload. Earlier two automation failures remain preserved; the successful focus-synchronized run did not reproduce a graph engine defect.

## Final Windows capture state

The Windows executable ran under Wine on the host RX 5300. This is useful executable/render coverage, not a test on the artist's physical Windows computer.

| Project | Resident / wanted | Loading / failed / critical pending | MID fallback groups | Additional evidence |
|---|---:|---:|---:|---|
| City | 185 / 185 | 0 / 0 / 0 | 3 | Authored static collision: 53 bodies; both source and countryside ground fields load. |
| Jungle | 175 / 175 | 0 / 0 / 0 | 62 | 12 wildlife actors, 3 species, 36 pose assets; no wildlife load failures. |
| Grandstone | 124 / 151 | 1 / 0 / 0 | 103 | 21 residents, 48 pose assets, 13 visible; 165 source fixtures; no resident failures. |

Fallback groups are the engine's usable MID geometry while optional HIGH refinement is unavailable, not rejected assets. Grandstone's short smoke ends with an optional streamed request still loading. Linux's longer run reaches 127 resident assets, also with zero failures/critical pending. These runs therefore do not prove complete HIGH residency or route-wide streaming stability. Visibility/shadow culling rejection counters likewise represent omitted invisible work, not failed resource loads.

The live-world dynamic physics-body count of four must not be confused with the city's separate 53 authored static collision bodies. Zero blocked-walk counters in an orientation capture also do not prove walkability: these runs do not traverse a walking route. Grandstone's previously documented source triangle walking/collision limitation remains open.

## Visual inspection

Viewed the actual final Windows `city.png`, `jungle.png`, and `grandstone.png`. All show the editor, textured environment geometry, and populated assets. No obvious missing-white-material fallback, large geometry hole, or failed render surface is visible. City paving is detailed and dry in this view; jungle canopy/path surfaces render; Grandstone's snow, rock, stone architecture, warm windows, and lava remain textured. Art quality and route coverage are separate from this resource smoke; these images are not an AAA-quality signoff. Asset cards currently use type icons rather than image thumbnails.

## Provenance and limits

- Six native editor functional cases passed on Linux binary SHA256 `acb2b124151ce3d2be79bae73e60c05728d1f552709f1129475edee1b6638c09`.
- Final Graph Compile/reload and three native orientation captures passed on Linux binary SHA256 `c0efcf5618f9f66cd3d62fe68d0296f629594cb3bfc72f9bbae00357110013b7`.
- Three final Windows orientation captures passed on executable SHA256 `264fb1fc9c2593850459136b8c0250b44791a8cfed3f244204310a85a5966717`.

This evidence supports opening and inspecting the three supplied environments and the tested editor workflows. It does not establish whole-game quest progression, full-route collision, sustained performance, physical-controller use, audible output, or behavior on an untested Windows GPU/driver. Existing feature/content limitations remain recorded in `PROJECT_STATUS.md`.
