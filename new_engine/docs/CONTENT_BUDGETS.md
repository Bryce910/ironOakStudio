# Content Budgets

This engine should be budgeted around visible frame cost, not a single raw polycount number.

The main constraints are:

- visible triangles
- draw calls
- shadow-casting geometry
- alpha-heavy foliage
- animated character count

Relevant engine context:

- terrain uses chunked LODs in `main.c`
- props and actors are frustum-culled
- terrain, props, player, enemies, and NPCs all participate in shadow rendering
- grass is instanced and alpha-heavy

## Asset Budgets

- Player character: `30k-50k` triangles
- Hero boss / major cinematic enemy: `35k-60k`
- Standard enemy: `10k-22k`
- Important NPC: `15k-30k`
- Background NPC: `6k-12k`
- Small pickup / clutter prop: `100-1.5k`
- Regular prop: `1k-6k`
- Large prop / statue / wagon / shrine: `6k-18k`
- Hero prop visible up close: `15k-35k`
- Tree trunk + canopy mesh: `2k-8k`
- Large showcase tree: `8k-20k`
- Rock: `500-4k`

## LOD Targets

- LOD0: `100%`
- LOD1: `40-55%`
- LOD2: `15-30%`
- Use billboards or very low proxies for far trees where possible
- Characters should have at least `3` LODs if they appear in groups

## Scene Targets

- Indoor room: `300k-900k visible triangles`
- Typical outdoor exploration view: `1.5M-3M visible triangles`
- Heavy outdoor combat scene: `2M-4M visible triangles`
- Stress ceiling: `~5M+ visible triangles` is risky in this engine, especially with shadows and foliage

## Draw Call Targets

- Indoor: `700-1000`
- Outdoor typical: `1000-1800`
- Stress case: `2000+` is risky unless most of the work is very cheap

Reducing material count and merging static content is usually more valuable than shaving small amounts of geometry off individual assets.

## Actor Targets

- Player: `1`
- Active enemies on screen: `6-20`
- Active NPCs on screen: `4-12`
- Combined animated characters in one view: target `10-25` before raising budgets further

The engine defines `NPC_MAX 16`, which is a useful baseline for keeping crowd scenes modest unless more optimization work is added.

## Foliage, FX, and Lighting

- Keep relying on instancing for grass
- Be conservative with alpha-heavy foliage
- Prefer tree silhouette quality over dense leaf geometry
- Keep overlapping dynamic lights limited
- Treat decals and particle FX as part of the frame budget, not free detail

## Review Thresholds

- Characters: hard warning above `50k`
- Standard enemies / NPCs: hard warning above `25k`
- Common props: hard warning above `8k`
- Hero props: review above `20k`, hard warning above `35k`
- Trees: review above `10k`, hard warning above `20k`

## Production Guidance

If the goal is AAA-looking results in this engine, optimize for:

- strong silhouettes
- good normals
- solid materials
- disciplined LODs
- low material count
- low draw calls
- controlled foliage density

This engine will look better with balanced content and good scene discipline than with ultra-dense meshes.

## Seldonne cinematic-pass exception

The cooked silver fir currently uses 65k triangles to preserve modeled branch/needle structure from a 685k-triangle source. This exceeds the generic per-tree guidance. It is a temporary, measured showcase exception, with near forest meshes capped at 28 and distant matched impostors. Retain the source for a hand-reviewed intermediate LOD; the remaining thin/noisy silhouette and transition quality are recorded in the production roadmap. The whole sample must still satisfy the target GPU frame budget; this exception does not raise budgets for every imported tree.

## Current grass field

The grass subsystem has a 180,000-blade hard budget over a 64 m radius, with distance-based density. Each curved ribbon submits 14 vertices / 12 strip triangles (the tip includes a degenerate triangle). An unmasked normal-density field at the test origin emits 157,207 blades; authored clearings, water and props reduce that count. The number is not a visible-triangle promise: the camera also rejects geometry outside its view. High paint densities use a deterministic reservoir over the entire circle rather than truncating one side when the cap is reached.

Two CPU instance buffers use about 13.7 MiB together, plus a 6.9 MiB GPU instance allocation at capacity. Uploads are staged and orphaned. The measured final grass pass averages about 1 ms at 1600×900 on the RX 5300; total frame time, alpha coverage and temporal stability remain the acceptance measures. The terrain and blades share density/style/tint masks so far cover does not require more geometry.
