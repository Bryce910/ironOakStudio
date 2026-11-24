# Live world and editor guide

This pass connects persistent scene identities to the running game's physics,
dialogue, training and NPC schedules, and enables automatic scenery residency.
It is a bounded playable integration in Valwood's own engine. Visual acceptance,
long route testing and performance acceptance are separate work; this guide makes
no 45–60 FPS claim. See [scenery streaming](../STREAMING.md) for the residency
implementation and its tests.

## Walk and interact

Launch `Launch_Valwood_World.sh` from the parent engine folder. **WASD** walks,
**Shift** sprints, right-drag turns the camera, and **V** switches between first
and third person. **Tab** opens the atlas, **F9** advances the demo tour,
**F10** opens graphics settings, and **F11** opens Studio. **Escape** releases
camera capture and leaves training. Interaction requires walking mode: **F**
toggles preview flight, in which **E** has a different movement function.

In the castle training yard, walk within roughly three metres of the existing
master of arms and press **E**. Choose **Begin training** in the conversation.
Choose attack direction with mouse movement or the arrow keys; **T** selects a
thrust. Left mouse attacks, right mouse holds guard, middle mouse thrusts and
**Space** dodges. Completing or cancelling a round reports the encounter result
back to gameplay, allowing another training round. The jungle camp steward also
has an **E** conversation. Dialogue releases the cursor and holds player movement
while a choice is open.

## What runs on a normal launch

Without `world.components`, a separate built-in registry supplies four castle
knights, two jungle guards/residents and two dynamic supply crates. The master of
arms and sparring opponent use the existing training facility. The six added
people use a real knight rig with skeletal animation; the crates use Jolt bodies
and full quaternion rendering. Main, reflection and shadow passes consume the
same live poses. The two crates also have two small static support colliders.

The default empty logical world receives six actors, two settlements and finite
supplies. Existing logical definitions and resource balances are preserved.
When reusing a world, bindings use explicit actor IDs and unique names; fewer
safe matches can mean fewer schedule-driven people. A uniquely identified base
resident replaced by a bound live actor is suppressed to avoid showing that
person twice. This does **not** make every member of the existing crowd a
schedule-driven skeletal actor.

Studio and the game share one authoritative living world and clock. Logical
schedules select home, work, leisure or guard destinations; nearby bound actors
physically walk or patrol toward those targets with terrain and collision checks.
Distant physical actors retain their positions, while the logical clock can
continue. Closing the program uses bounded catch-up on the next launch; the
program does not keep executing after it closes.

## Author bindings in Studio

Open **F11 → Tool → Live components** (tool 24). The panel shows active state,
bindings, runtime diagnostics, the separate built-in registry and recent typed
host requests. Scene entity IDs, physics body IDs and logical actor IDs are
independent; the binding table connects them explicitly.

1. Apply the scene draft first. Author bodies, dialogue/rules and logical
   schedules in their existing Physics, Gameplay and Living world tools.
2. Bind a scene entity to a body, or an authored NPC to a logical actor. A mesh
   pivot offset aligns a centre-based body to its rendered model. Schedule
   bindings have separate home/work/leisure/guard targets and walking speed.
3. Press **Configure current Studio definitions** to activate those edits.
   Pending binding-table changes do not alter the active runtime. Invalid
   references or renderer capacity are rejected before replacement.
4. Save the typed subsystem assets, then save the `.components` manifest using
   project-relative `.physics`, `.gameplay` and `.living` paths. The manifest
   references those assets; it does not contain them.

**Export built-in demo set (new files)** creates `runtime-demo.physics`,
`runtime-demo.gameplay`, `runtime-demo.living` and `world.components` only when
those destinations are absent. It preserves existing files and `world.studio`.
The fallback markers are not silently inserted into the user's scene document.

Scene Apply checks configured references before committing geometry. Removing a
bound entity, or scaling a physical mesh without updating its collider, requires
explicit binding/collider repair. Ordinary scene refresh preserves existing live
poses. Resetting the logical world to different actor IDs requires reconfiguring
its bindings before entering Play.

## Play, Stop and saves

Use Studio's **Play scene**, then reopen Studio with **F11** and choose **Stop
play**. Both Stop choices restore the live component snapshot: body poses and
velocities, activation, gameplay state, logical balances/schedules and live
animation state. **Keep authoring changes** retains scene edits made during Play;
**Discard play changes** restores the pre-Play live scene while retaining any
separate draft that was already unapplied before Play. Subsystem authoring files
retain their own Save/Undo workflows.

Restoration passes through normal Apply. Gameplay and project Save remain held
until restoration succeeds; a failed transaction retains the previous live
scene and exposes repair/retry instead of publishing half a scene. Project Save
is blocked during Play. Configuring a different runtime is also blocked until
Play/Stop is complete.

## Streaming and present limits

The normal world automatically streams high-detail scenery groups on approach.
Defaults are one CPU worker, **512 MiB** prepared CPU data, **192 MiB** logical
geometry residency/reservations, **1,536 MiB** tracked shared textures and **2 MiB**
of owner-thread uploads per frame. Near groups preload 256 metres before their
draw interval and unload beyond 384 metres of hysteresis. Shared textures are
counted once. These counters are not an exact total-VRAM measurement or a fixed
millisecond upload guarantee.

Mid/far or single-LOD fallback meshes, exact source bounds and authored
colliders remain available. Residency is per asset alias/group, rather than
independent instance tiles. Destination travel holds gameplay until terrain and
other required scene resources are ready and immediate high-detail demand has
finished; unrelated background prefetch need not finish. Failed admission uses
the available fallback and diagnostics instead of an endless loading hold.

The live renderer currently admits up to 64 bound actors and 32 physical models,
including the built-in actors/props. Nearby motion uses local steering and
collision checks, not general world-scale route planning. General hostile
encounters need an additional combat host; the supplied host dispatches the
castle training encounter. Arbitrary new character rigs, full locomotion and
combat state authoring, comprehensive foot placement, broad production recovery
across every separate asset, and complete-world performance certification remain
outside this slice. More asset purchases are not needed to test these connections.
