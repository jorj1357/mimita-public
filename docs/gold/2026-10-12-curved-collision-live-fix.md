# Gold record: exact-triangle collision made fast on AtDM

Date: 2026-10-12

## Purpose

This document is a reusable explanation of how a severe curved-geometry
collision stall was investigated with the live-fix method, how wrong causes
were ruled out, what code was changed, and what the real executable proved.

It is a record of observable engineering reasoning, not private hidden
chain-of-thought.

## The simple version

The game was not slow because cylinders have a special “cylinder cost.” The
game was slow because the old collision code asked too many questions:

> “Does each of thousands of avatar triangles touch each of many nearby map
> triangles?”

Long triangles from large transformed map objects had big boxes around them.
Those boxes made many triangles look possibly nearby. Boomer had 3,680 actor
triangles, so the number of possible pairs became hundreds of thousands.
The exact triangle math was then repeated during correction and movement
substeps.

The fix keeps the original triangles. It adds better indexes around them:

1. Build one reusable tree over the avatar's original triangles.
2. Keep the map's triangle index and cache actor/world pair relationships.
3. Reject whole groups before exact triangle math.
4. Skip actor triangles whose cached pair list is empty.
5. Run the original exact triangle contact test only on the remaining pairs.

That changed the live Boomer AtDM path from about 0.20 FPS and multi-second
frames to commonly 242–419 FPS and 2.38–3.92 ms one-second frame averages,
with `contact_count=1` and `actor_avatar=monstertest1` still present in the
journal.

## Investigation timeline

### 1. Measure the real failure

The live AtDM mode-11 run used the teleport position `-43,-28,290` and the
`m1` startup command. Logs showed:

- AtDM had about 50,859 collision triangles.
- Jason had 72 actor collider triangles and still fell to about 5.7–5.9 FPS
  in the hotspot.
- Boomer had 3,680 actor collider triangles and fell to about 0.20–0.26 FPS.
- Boomer solves reached about 200,000–400,000 tests, with individual solves
  around 100–249 ms and old frame windows in seconds.
- `fly 1` kept both avatars around 245–390 FPS and 2–3 ms frames.

This made fixed-tick collision the primary suspect, not rendering.

### 2. Repair the crash that hid the measurements

The access violation at `mimita.exe+0x427b3c` resolved to
`collectActorMeshContactsInto`. The pair-count lookup and pair-pointer lookup
disagreed about whether a triangle was valid. A positive count could be
returned with a null pointer range.

The lookup was changed to validate the requested mesh/triangle and the CSR
range itself. After a fresh Dev Loop build, the former null read no longer
occurred. This was a correctness prerequisite; it was not yet the FPS fix.

### 3. Remove repeated equivalent work

The default movement collision substep count was reduced from six to one,
while the adaptive travel-distance safety remains available. This improved
the old Boomer result but left 663–1,127 ms frames and 200,000–400,000 tests.
The experiment therefore proved that six substeps multiplied the work but did
not explain the work explosion.

### 4. Add an actor-side reusable tree

The actor tree is created once per loaded collider vector and reused. The tree
stores original IDs and bounds only. It does not alter the `.glb`, visible
vertices, triangle normals, or final contact operation.

The actor tree is transformed conservatively for the previous and desired
poses. A node can be rejected as a group. This avoids transforming and
testing every actor triangle against the same world region.

### 5. Use pair-cache early rejection

The narrowphase now reads the cached actor/world pair range before doing the
expensive per-triangle transform and exact work. Empty ranges exit immediately;
non-empty ranges contain the original world triangle IDs. The old world-tree
fallback remains for callers that do not have a pair cache.

This was the decisive live improvement. Build 2179's journal showed normal
exact-test counts such as 19, 130, 179, 311, and 954 instead of the old
200,000–400,000 range. A busy 3,767-test sample took about 5.35 ms; ordinary
samples were about 1.04–2.51 ms.

## Before-and-after evidence

| Measurement | Before | Current live result |
| --- | ---: | ---: |
| Boomer FPS at old hotspot | 0.20–0.26 | commonly 242–419 |
| Boomer frame time | 3.9–5.1 seconds | commonly 2.38–3.92 ms averages |
| Exact tests per solve | 200,000–400,000 | commonly 19–954; busy sample 3,767 |
| Collision solve | 100–249 ms common | commonly 1.04–2.51 ms; 5.35 ms busy sample |
| Avatar | `monstertest1`, 3,680 triangles | same avatar and triangle count |
| Contact result | overloaded / budget-limited | `contact_count=1` in representative samples |
| `.glb` assets | unchanged | unchanged |

The old six-substep multiplication was not merely hidden; the current
collision summary also records the actual candidate counts, tests, contacts,
avatar, and timings in `events.jsonl`.

## What the fix does not claim

- It does not claim every frame on every PC can never exceed 4.17 ms.
- It does not claim an unrelated simulation hitch cannot occur. One live
  aggregate had a 979 ms simulation hitch while collision averaged 0.45 ms.
- It does not claim every curved object is inherently expensive.
- It does not replace exact collision with a box, capsule, sphere, or proxy.
- It does not edit or re-export any `.glb`.
- It does not use build success as proof of runtime behavior.

The supported claim is narrower and stronger: the old AtDM large-avatar
collision pair explosion is fixed in the current live build path, and exact
triangle contacts still occur.

## Source owners

- `src/physics/movement/physics-collision-shared.h:555-562`: safe CSR pair
  lookup that prevents the null-range crash.
- `src/physics/movement/actor-triangle-solver.cpp:84-128`: reusable actor
  triangle tree.
- `src/physics/movement/actor-triangle-solver.cpp:846-880`: bounded live
  collision summary.
- `src/physics/movement/physics-collision-mesh.cpp:498-554`: cached-pair
  lookup and empty-pair early skip.
- `src/physics/physics-mini.h:30`: one default fixed-tick collision substep.

## Live-fix method

The method was:

1. Read the collision and logging specifications.
2. Add bounded owner-level logs.
3. Publish a fresh executable through Dev Loop mode 11.
4. Run the real AtDM map with Jason and `m1`/Boomer.
5. Read `events.jsonl` for FPS, frame time, physics time, avatar identity,
   candidate pairs, exact tests, contacts, and crashes.
6. Change only the owner supported by the evidence.
7. Rebuild through Dev Loop and repeat.
8. Keep the exact-triangle contact path and reject only broadphase work.

## Model-setting note

This work used GPT-5.6 Terra at high reasoning effort. The comparison named
by the developer was GPT-5.6 Luna at medium reasoning effort. The output here
is consistent with Terra/high being useful for a long evidence-driven task,
but this is not a controlled experiment: the prompts, accumulated context,
repository state, logs, and model settings all differed. The defensible proof
of improvement is the runtime journal and source behavior, not the model label.

## Final status

The collision-heavy AtDM Boomer path is strongly supported as fixed in build
2179. The loop was later replaced by a different mode-1 run, so a fresh final
human acceptance pass should still launch mode 11 through the Dev Loop, keep
`m1`, verify the exact teleport, and watch a new 60-second journal window.
