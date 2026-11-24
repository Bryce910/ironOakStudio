# Authoring and demo completion pass

1. Ray-based object selection, viewport translation/yaw/uniform-scale handles, multi-selection, duplicate and persistent groups. One drag is one undo transaction; drafts show bounds until Apply commits grounding/collision.
2. CPU asset/material diagnostics, project-managed static GLB import/reimport, editable LOD/collision definitions and rollback after failed preparation. Preserve source files and keep import work outside ordinary walking.
3. HDR panorama import, environment rotation/intensity, diffuse irradiance and filtered specular reflections, with safe shader/sampler binding and bounded loading costs. Retain rainforest weather and local probes.
4. Refine the castle/tavern and jungle using the existing assets, then run complete native routes in both cameras with saved settings. Record 1% lows, individual budget misses and GL/ownership errors.

NPC/navigation, effect/water editors and shader graph follow this pass. No migration to another engine.
