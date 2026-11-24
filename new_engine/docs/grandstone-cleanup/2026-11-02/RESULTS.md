# Grandstone stairs, doorways and rendering cleanup

The installed Linux Grandstone playtest now supports both hall side stairrooms and their upper landings, both barbican staircases, and the complete 67-piece exterior staircase. Movement no longer treats adjacent trim, stone joints or separating doorway contact as a new blocking step. Closed source gates remain solid. The original architecture and art banks were preserved.

The close camera uses a 0.12 m near plane with matching depth reconstruction in effects, fog, occlusion and temporal rendering. A private Grandstone shader correction removes duplicate material occlusion from already baked diffuse lighting, restoring light on black stair undersides. Environment/specular occlusion and direct lighting retain their existing behavior. Source worn/cratered stone textures and plain source undersides remain visible; this does not certify finished art quality.

## Acceptance

| Check | Result | Scope |
| --- | --- | --- |
| Native actual movement | PASS | Nine routes, both directions, 555.548773 m, zero blocked/dropped steps |
| Windows actual movement under Wine | PASS | Four selected routes, both directions, 95.834524 m |
| Solid gates and gravity | PASS | Two gates tested from both sides; ordinary 1.087 m jump and supported landing |
| Shading comparison | PASS | Four native routes repeated with the accepted shader; actual endpoint screenshots reviewed |
| Native and Windows combat | PASS | Ten cases and 42 tagged SDL events each; actual defeat, retry, victory, parries, VFX, Continue exploring and reset |
| Installation and source audit | PASS | New runtime, six sector files and one private shader installed; original source projects and ordinary runtimes retained |
| Windows add-on | PASS | All 18 ZIP members, dependency closure and exact tested runtime/data/shader binding verified |

Movement cases use explicit setup teleports between independent stairs/doorways and between the two sectors. Every measured step uses the normal movement adapter. These are regression routes, not one continuous journey through the entire castle. The full native movement run preceded the shader-only correction; the four-case native shading run and both combat runs accepted the final shading. The runtime and controller code remained identical.

The focused CPU tests cover real mesh joints, doorway overlap recovery, inward wall rejection, step/drop limits, airborne headroom, source storeys, bounds and query allocation behavior. Detailed reports are in physics/, adapter/ and geometry/.

## Performance and remaining scope

On the RX 5300 at 1600×900 and fixed 70% scene scale, the whole final combat runs averaged **38.42 FPS native** and **39.63 FPS under Wine**. Native 1% low was 14.62 FPS; worst frame was 417.44 ms, including loading. These are workflow measurements rather than a paired optimization benchmark or a 45 FPS floor. Frame pacing remains unfinished.

The hall and snowy approach remain separate playable sectors with pause-menu travel. Distant unconnected castle districts are outside this collision delivery. The current prototype hero and repeatable practice opponent remain; this pass does not add equipment, rewards, new quests or new character assets. The source asset's flat undersides and floor wear still need an artist's presentation pass. Ordinary jump landing is verified in the executable; low-ceiling impact rejection is covered by the CPU fixture. Audible sound and controller feel still need human review, and the designer's physical Windows machine has not been tested.

## Open and hand off

The normal Grandstone session was reopened using [the direct launcher](/home/computer/Desktop/final_engine/Launch_Valwood_Playtest_Grandstone.sh). It starts in the first hall with the armed hero. Press Esc > Travel to snowy entrance for the exterior stairs. Normal audio and Logitech F310 input are enabled; QA replay is disabled.

Use [Valwood_Windows_Grandstone_Fix_2026-11-02.zip](/home/computer/Desktop/final_engine/Valwood_Windows_Grandstone_Fix_2026-11-02.zip) with a fresh extracted copy of the original orientation transport. This is a Grandstone-only add-on and does not require the older combat add-on. Extract it into that transport folder, then run Install_Playtest_grandstone.bat with a new output folder. It supplies playtest-index.json, Install_Playtest_Bundle.py and Install_Playtest_grandstone.bat. Run Launch_Playtest.bat inside the installed seed. No Unity or separate Python installation is needed.

The add-on is **13,160,342 bytes**, SHA256 `e93a574e2c0b58c79c138d37a3e42f431aa72ef04a91b3a166f9ba3fb3b249cb`. It materializes an independent approximately 24.54 GB seed. Existing seed folders are preserved. Previous orientation, traversal and combat ZIPs remain available and unchanged. Use the original orientation workspace for art editing; gameplay copies retain authoring guards.

## Evidence

[Final manifest](FINAL_REVIEW.json), [native movement](native-obstacles-final/results.json), [Windows movement](windows-obstacles-final/results.json), [shading review](render/baked-diffuse-candidate/REVIEW.md), [native combat](native-combat-final/results.json), [Windows combat](windows-combat-final/results.json), [installed files](runtime-install.json), [source audit](source-audit-final.json), [Windows archive verification](windows-overlay-verification.json), and [human checklist](PLAYTEST_CHECKLIST.csv).

The audit rechecks 40 original state files and 605 source-guard records, private-copy isolation and bank link targets. It does not rehash entire large asset banks or repeat a full production-seed installation on this low-space Linux computer. The installer compared all 201 other private Grandstone files before and after installation; the independent saved baselines cover 200 of them, with the editor layout covered by the installer comparison. Rollback copies are retained under before/installed/.
