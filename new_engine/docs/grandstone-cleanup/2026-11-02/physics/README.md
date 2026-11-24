# Grandstone walking controller repairs

The previous controller has three reproducible failures in the native Jolt fixture:

1. Its eight support rays extend 40 cm from a capsule whose radius is 37 cm. A 30 cm tread 39 cm away therefore raises the apparent floor, and even movement parallel to that clear ledge rejects the player's current feet.
2. A floor ray exactly on a tread/riser seam can return the vertical riser normal and reject the adjoining tread. A dense 5 cm approach reproduces this at the step's front edge.
3. A capsule overlapping a doorway jamb by 2 cm rejects movement away from it. The original closest-hit sweep treats its initial contact as a new blocking contact, independent of the movement direction.

`controller-regressions.json` records each failure using the preserved original Support header and the same current native Jolt adapter, followed by a pass of the expanded fixture with the repaired controller. No GPU or keyboard simulation is involved in this CPU fixture.

The repaired support query establishes a source floor and current storey with the centre ray, continuing below at most eight steep source faces and resolving an exact seam with four 2 mm neighbouring source rays. A downward cast of the actual rounded capsule supplies its physically supported height; the oversized support ring is removed. Detailed stairs also contain real stone joints: the centre ray can pass through a joint and hit a much lower storey. A walkable source tread within 10 cm of the capsule's actual contact height then supplies the floor reference, while the rounded capsule height remains separate. If the contact itself falls on a joint, a bounded 2/5 cm local source probe searches inside the actual capsule footprint. These probes select the tread reference and do not set the physical height. Real voids without capsule contact remain unsupported.

Point tread height changes separately enforce the 60 cm climb and 80 cm drop limits, so the rounded footprint cannot turn a taller ledge into a series of permitted tiny steps. The native movement-only capsule query discards contacts that do not oppose movement before closest-hit selection and supplies Jolt's active-edge movement hint; generic camera/physics casts retain their original contract.

The expanded fixture covers a five-tread staircase with 140 frame-sized ascent/descent moves, an exact riser seam, a steep bevel, a physically supported 2 cm stone joint, lateral and diagonal movement around a tread, clear parallel ledges, doorway overlap recovery and tangent sliding, inward wall rejection, a second wall encountered while leaving the first, mesh jamb escape on both sides, a permitted 70 cm descent, rejected 70 cm climb and 90 cm descent, low overhead clearance including upper-floor undersides, overlapping storeys, jumps, real voids, hard sector bounds and atomic malformed/spawn rejection. Jolt query heap fallback remains zero.

The original actual-source guided routes continue to pass: 31 m/620 dense samples in the hall and 10.928 m/219 dense samples in the approach, including unchanged closed-door/column, resident, void and bounds negatives. These route reports certify those routes only.

The expanded actual-source gate in `../geometry/obstacles-final.json` also passes all nine routes in both directions: both open hall doorways, both lower and upper hall staircase flights, both barbican staircases, and the complete 67-piece exterior staircase. It covers 555.549 m and 11,542 dense 5 cm moves. Each accepted move is resampled twice to verify stable support within 5 mm. Closed source gates and out-of-bounds positions remain blocking. The main executable's native rehearsal is a separate integration check.

Reproduce the focused fixture from `new_engine`:

```sh
g++ -std=c++17 -O2 -Wall -Wextra -Werror -I. \
  tests/world_playtest_walk_test.cpp engine/world_physics.o build/physics/libJolt.a \
  -pthread -o /tmp/valwood-world-playtest-walk-test
/tmp/valwood-world-playtest-walk-test
```
