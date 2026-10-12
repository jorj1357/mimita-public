# Curved collision runtime evidence

Date: 2026-10-11

## Request

Investigate the reported 5–10 FPS drops near cylinders, spheres, and other curved or non-box geometry using the live logging workflow. Do not implement a collision change until the runtime evidence identifies a safe owner and fix.

## Evidence

- Followed `docs/ROUTER.md`, the collision specification and architecture documents, the runtime-scenario workflow, and `docs/specs/debug-logging/AI-fix-live.md`.
- Published and ran the fresh dev-loop executable `C:\mimita-v9\.dev\builds\2071\mimita.exe`. The matched AtDM run was `20261011_213557`, with journal `C:\mimita-v9\logs\2026-10-11\20261011_213557\events.jsonl`.
- The AtDM client emitted `collision.solve.summary` records from `solveActorTriangleCollision`. Representative records had 21–23 world candidates, 47,175–137,410 triangle tests, and 47–137 ms solve time. The actor diagnostic reported 6 meshes and 3,680 actor collider triangles; the 200,000-test guard was not reached in this run.
- The earlier matched AtDM run `20261011_212631` reached 200,000–800,000 tests across correction iterations and recorded approximately 117–493 ms solve times. This confirms the same owner can become substantially worse in a denser candidate region.
- The map load record reported `assets/maps/atdm.glb` with 50,859 collision triangles. Rendering scopes in the same journal were materially lower than the collision solve spikes, so the current evidence points to CPU actor/world collision narrowphase rather than a pure render-GPU cost.
- The standalone version identity check was run against `C:\mimita-v9\.dev\builds\2068\mimita.exe --versioninfo`; it produced a fresh version-information journal and identified the executable path and build timestamp. Build success and runtime evidence are recorded separately here.

### Matched AtDM A/B evidence

The follow-up runs used dev-loop mode 11, the AtDM map, the mode-11 spawn/teleport setup, and fresh executables. FPS was read from runtime `PERFORMANCE_FRAME` records, not inferred from build status or rendering time.

| Case | Journal/build | Direct runtime result | Collision evidence |
| --- | --- | --- | --- |
| Jason, normal movement | `20261011_214907`, build 2085 | About 7.9–11.3 FPS; 88–126 ms frames | 6 actor meshes, 72 actor triangles, 61 world candidates, about 4,331 triangle tests per call |
| Jason, `fly 1` | `20261011_215544`, build 2087 | About 245–340 FPS; roughly 3 ms frames | Physics 0 ms; no collision events |
| Boomer, normal movement | `20261011_215818`, build 2090 | About 0.20–0.26 FPS; 3.9–5.1 s frames | 6 actor meshes, 3,680 actor triangles, 64 world candidates, 200,000 tests per call; six calls per fixed tick |
| Boomer, actor-triangle solver disabled | `20261011_220019`, build 2091 | About 0.23–0.32 FPS; 3.1–4.4 s frames | The severe physics stall remained |
| Boomer, normal movement with one substep | `20261011_220528`, build 2095 | About 0.89–1.51 FPS; 663–1,127 ms frames | One call per tick, but each call still reached 200,000–400,000 tests |
| Boomer, `fly 1` | `20261011_215707`, build 2088 | About 245–340 FPS; roughly 3 ms frames | Physics about 0 ms; no collision events |

The scope-split run `20261011_220250` (build 2093) showed Boomer normal movement at approximately 4.1–4.4 second frames: `PhysicsMainUpdate` maxima were about 723–926 ms while `AimBodyUpdate` was about 0.7–1.4 ms and rendering was about 0.4–0.9 ms. This rules out aim-body update and rendering as the primary stall owner.

The latest bounded geometry diagnostic was `20261011_220747` (build 2097). In the Boomer normal case, every sampled collision call had 64 candidates and 64 large candidates, a maximum candidate AABB span of about 222 world units, and 200,000 triangle tests. The call itself took about 102–158 ms, and six calls were performed per fixed tick. This is the strongest current evidence that long, spatially broad AtDM collision triangles are defeating the normal candidate/narrowphase path. It also explains why a dense Boomer collider makes the existing world geometry failure much worse, while Jason still drops badly with only 72 actor triangles.

The previous cylinder investigation recorded the same geometry pattern in another map: scaled cylinder source nodes produced very large transformed collision triangles. The current AtDM runtime data is consistent with that pattern, but it does not yet identify the exact AtDM source node or prove that one specific triangle should be subdivided.

### Exact teleport-location comparison

The original mode-11 argument list did not dispatch the teleport; its journal showed only `help`. I corrected this temporarily for a controlled run, and the journal then confirmed `teleport -43, -28, 290` was dispatched after the AtDM world loaded.

- Boomer exact-location run: `20261011_221950`, build 2103. The actor solver reported position `[-43, -28, approximately 290]`, 127 candidates, 126 large candidates, 3,680 actor triangles, and 400,000 tests in the first correction. The first solve was 234.6 ms; later collision frames were commonly 108–135 ms. Sustained performance windows reached 0.21 FPS, 4,814.6 ms average physics, 4,823.2 ms simulation, and 0.446 ms rendering.
- Jason exact-location run: `20261011_222119`, build 2107. The same teleport was dispatched. Early collision calls had 64 candidates and 4,608 tests; after movement into the nearby hotspot, the solver repeatedly reported 114 large candidates and approximately 7,099 tests, with 5–7 ms solve time per call. Six calls per fixed tick summed to approximately 26–30 ms before the frame began catching up. Sustained windows were approximately 5.7–5.9 FPS, 157–164 ms average physics, and 0.53–0.65 ms rendering.
- The Jason run also recorded the avatar evidence in its live journal; the collision fields were 72 actor triangles, not the Boomer's 3,680. This rules out Boomer rendering or Boomer mesh density as the root cause of Jason's drop. Boomer is an extreme multiplier on the same broad world-candidate problem.

The source asset inspection independently found 143 AtDM nodes and 137 meshes, with many very large transforms: for example, cylinder nodes with scales around 106, 130, 162, and 211 world units, and sphere/icosphere nodes with scales around 206–288 world units. The loader currently derives collision directly from the expanded render mesh in `src/map/map-loader-collision.cpp`, so these transformed features become collision triangles. This is strong source-side support for the long-triangle hypothesis, but the current runtime journal still does not attach a candidate triangle index back to a specific GLB node.

One important interpretation correction: many hot collision records report `grounded:false`. The failure should be described as the actor/world collision and correction path during movement/fall, not only as grounded-floor walking. Networking remains small in the low-FPS windows, while physics dominates and rendering remains sub-millisecond.

## Diagnostic changes

- Added bounded actor mesh count, actor collider triangle count, avatar identity, test budget, and budget-hit fields to the existing verbose collision summary/frame events in `src/physics/movement/actor-triangle-solver.cpp` and `src/physics/movement/actor-triangle-solver.h`.
- Added resource-window fields for existing performance counters in `src/perf/perf-frame.cpp` and `src/perf/perf-frame.h`. Those counters remained zero for this hot actor-triangle path, so they were not used as the deciding evidence.
- Temporarily enabled collision verbose logging for the run, then restored `config/debuglogger.json` collision logging to `off`.

## Decision

The grounded physics/collision path is proven to be the trigger: both Jason and Boomer become fast in `fly`, and both fly runs retain the same AtDM map while bypassing grounded collision. The world geometry is a major contributor because the hotspot repeatedly presents large, broad candidates, and Jason remains slow with a very small actor collider. Boomer amplifies the cost because its 3,680 actor triangles hit the per-call test budget.

Reducing the default six substeps and disabling only the new actor-triangle solver do not solve the issue. Therefore the best current fix target is the world collision representation/candidate path, not an avatar-only change, a global test-budget increase, or simply deleting the new solver. No gameplay fix has been implemented; the diagnostic patch remains bounded. A safe fix still needs a controlled geometry A/B that preserves collision behavior while testing subdivision or source-authored/simplified collision for the long AtDM features.

## Acceptance status

- Source/build evidence: complete for the diagnostic patch; dev-loop published builds 2068–2071 successfully.
- Runtime evidence: extended with matched Jason/Boomer normal-vs-fly runs, solver-off and substep A/B runs, scope attribution, and candidate-span measurements; journals were inspected.
- Human visual/FPS acceptance: not claimed; the automated live run did not replace a human replay of the exact cylinder/sphere route.
- Regression: none confirmed by this investigation.

## Implementation follow-up

- Added an actor/world AABB-tree pair cache in `src/physics/movement/collision-aabb-tree.h` and `src/physics/movement/actor-triangle-solver.cpp`. The cache keeps original actor and world triangle IDs, so it changes candidate discovery only; the existing exact triangle contact code remains authoritative.
- Added cached triangle-oriented broadphase prisms in `src/physics/physics-types.h` and `src/map/map-loader-collision.cpp`. `src/physics/movement/physics-collision-mesh.cpp` uses them as a conservative rejection before `triangleTriangleIntersect` and rounded-feature tests. Exact visible GLB triangles are unchanged.
- Fixed the empty-pair fallback in `ActorTriangleBroadphase`: a triangle with zero cached world pairs now performs zero scans instead of falling back to all world candidates.
- Build 2122/2123 included the oriented-prism path. Its live AtDM journal showed approximately 335,700 of 335,700 AABB pair attempts rejected before exact tests, with about 6–106 exact triangle tests instead of the 200,000 budget. That run still had 58–258 ms solves because the first implementation was measuring a fallback path; it was not accepted as a 4 ms result.
- The subsequent custom recursive tree-pruning experiment caused the client to stop advancing after teleport and was removed from the active path. This is recorded as a rejected experiment, not a fix claim.
- The empty-pair correction was compiled and linked successfully in the final direct build (`BUILD SUCCESS`, 50 compiled / 499 skipped, 193.71 s). A fresh mode-11 runtime was started with build 2133, but its client journal stopped after startup before producing a controlled collision summary, so 4 ms or 240 FPS acceptance is not claimed.

## Current decision

The implementation is source/build complete but runtime-incomplete. The evidence supports the actor/world candidate explosion and shows the oriented prism can remove nearly all expensive exact triangle tests, but the final empty-pair build still needs a clean matched Jason/Boomer mode-11 run before it can be called the best current solution or accepted against the 4 ms target.

## Mode-11 dev-loop retest

- Ran the requested command path through `devscripts/dev-loop.py` with launch mode 11 and the configured teleport `teleport -43, -28, 290`.
- Fresh runtime identity was build 2138, run `20261011_232649`, journal `C:\mimita-v9\logs\2026-10-11\20261011_232649\events.jsonl`, map `atdm`, and the client/server executable hashes matched.
- The client journal confirmed both the initial map load and the AtDM reload: 50,859 collision triangles and 56,500 render triangles. The mode-11 server remained healthy at approximately 2.7–3.6 ms loop average, with zero server-side triangle queries; this keeps the reported stall on the client-side actor/world path rather than the server.
- Before the avatar command could be driven to a controlled Jason/Boomer comparison, the game-window control was interrupted by a physical Escape key. Therefore this retest does not claim Jason or Boomer acceptance. It produced only startup performance windows (approximately 302 FPS while settling and 76 FPS during the network/map transition) and zero `collision.solve.*` records.
- The temporary collision logger enablement was restored to `off` after the run. The mode-11 run was stopped through the dev-loop control path.

This run is additional launch/build evidence, not a successful fix result. A useful next run must keep mode 11, enable the collision category before launch, and exercise `xxx` and `m1` long enough to capture `PERFORMANCE_FRAME` beside `collision.solve.summary`/`collision.solve.frame` for each avatar.

## Repeated client access violation evidence

- The requested live dev-loop session was initially on mode 1 (`badhouse`), so it was stopped and replaced with mode 11 (`atdm`). The mode-11 controller remains running.
- Fresh mode-11 build 2143 journal: `C:\mimita-v9\logs\2026-10-12\20261012_000341\events.jsonl`.
- The client crash report is `C:\Users\guita\AppData\Local\Mimita\crashes\crash-2026-10-11_20-04-02-34004.txt`. It records `ACCESS_VIOLATION (0xc0000005)`, a null read at `mimita.exe+0x427b3c`, and the same stack offset previously seen in the screenshot.
- `events.jsonl` does contain the client’s preceding evidence: AtDM loaded with 50,859 collision triangles, a 7,059.7 ms maximum frame during map transition, a 1,928.9 ms maximum frame with networking at 1,917.0 ms, and the teleport dispatch. It does not contain a final `crash` event for this access violation because the crash handler currently writes the text report/minidump separately and the process terminates before a structured crash record is appended.
- This particular mode-11 crash produced no `collision.solve.*` records and no avatar-load record. The last categorized spike was `State`, followed by `Networking`; the server continued running at roughly 3–5.7 ms outer-loop averages with zero server-side triangle queries. Therefore this crash is not yet proven to be the curved-collision implementation.
- Added the existing `xxx` alias to mode 11 as a second startup command for the next clean run. The already-running daemon had loaded the older mode configuration, so the current 2143 journal did not actually execute `xxx`; a controller restart is required before treating that command as active.

## Pre-existing worktree changes

The avatar files, imported boomer GLBs, avatar/debug documentation, and unrelated config changes already present in the worktree were preserved and are not attributed to this investigation.
