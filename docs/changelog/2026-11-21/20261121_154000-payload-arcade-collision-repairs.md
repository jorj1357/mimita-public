# Payload Arcade Collision Repairs

# Task

- Task ID: payload-cart-arcade-contact / payload-arcade-npc-grounding
- Session ID: work-f0fe497a9427
- Owner: payload-arcade-collision-fix
- Focus: Arcade collision with the Payload cart and NPC grounding
- Summary: Filed the requested Payload and AI-testing issues; added dynamic physical-box sweeps to the legacy Arcade capsule path and broadened the bounded NPC spawn recovery search. The source was captured by the automatic DLL queue. Runtime behavior remains unproven.
- Status: BUILD_PENDING_RUNTIME_UNPROVEN
- Date, time, timezone: 2026-11-21T15:40:00-05:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: d651e1332fccff70bc5cc4601dc24d55784820b5
- Final commit: d651e1332fccff70bc5cc4601dc24d55784820b5

# Pre-existing changes

- Exact status output: `git status --short` showed extensive pre-existing shared-checkout edits across config, devscripts, docs, and source, plus other agents' issue/changelog additions. This session only edited `src/physics/physics-types.h`, `src/physics/movement/physics-collision-glb-sweep-slide.cpp`, and `src/network/server-npcs.cpp`, and created the eight issue records listed below and this changelog.
- Files not created or modified by this session: all other paths in the shared working tree. No unrelated changes were reverted or attributed to this task.

# Requested behavior

Preserve the user's report in the eight issue records. The immediate fixes are: make the Payload cart collidable in Arcade mode and attempt to stop Payload Arcade NPCs falling through the map. Follow-up requests cover stable and legacy moving-object carry modes, a configurable pulsing through-wall cart outline, smooth moving-object interpolation, live inspection of existing `events.jsonl` runs, Payload server tick-rate investigation, and a low-CPU/headless reproduction route.

# Specification alignment

- Current specification paths: `docs/ROUTER.md`; `docs/specs/gamemodes/payload.md`; `docs/specs/movement/movement.md`; `docs/architecture/collision/collision.md`; `docs/architecture/player-npc-systems/npc-movement.md`; `docs/specs/npc-behavior/npc-behavior.md`; `docs/specs/dev-loop/dev-loop.md`; `docs/specs/dev-loop/agent-proof-and-verification.md`; `docs/workflows/issue-intake-and-triage.md`.
- Exact requirements: keep gameplay collision in the fixed 60 Hz simulation; preserve cached broadphase use and avoid per-query allocations in hot loops; Payload cart collision is authoritative server gameplay; build success alone is not runtime proof.
- Why the change follows the specification: the Arcade legacy capsule sweep previously only consumed static world triangle candidates. The edit reuses each physical box's cached entity surface and filters candidates by its swept bounds before running the capsule triangle sweep. NPC spawn candidate search remains bounded at 25 ms.
- Conflicts or decisions: none. The user's image is treated as evidence of the displayed `SERVER RX: 42.9 Hz` and `ACTUAL: n/a`; it does not prove the server simulation tick rate. The follow-up rate issue explicitly asks for investigation.

# Exact implementation changes

## File: `src/physics/physics-types.h`

- Lines/functions/headings: `SweepHit` (around line 72).
- Old content or behavior: sweep contacts only identified a static collision triangle.
- New content or behavior: carry `entityId` and `surfaceVelocity` with a sweep result so the response can distinguish and resolve contacts against moving entities.
- Reason: physical entity triangles are not indexed in the static world collision mesh.
- Why unrelated behavior is preserved: static hits retain the zero entity ID and existing static contact response.

## File: `src/physics/movement/physics-collision-glb-sweep-slide.cpp`

- Lines/functions/headings: `doGLBSweepSlide` (around lines 71–875); dynamic box candidate pass around line 265; support velocity bookkeeping around line 873.
- Old content or behavior: the legacy Arcade root-capsule sweep queried static triangles only, so the Payload cart could be passed through.
- New content or behavior: in Arcade mode, broadphase-cull physical boxes against the swept capsule bounds, query their cached world triangles, sweep in relative motion, respond relative to entity velocity, and preserve a moving support ID/velocity. Emit a rate-limited `collision.arcade_entity.sweep` event on contact.
- Reason: include the physical Payload cart in the contact path without replacing the static world broadphase.
- Why unrelated behavior is preserved: dynamic collision is restricted to actor-colliding box entities in Arcade mode; the static world sweep remains in place.

## File: `src/network/server-npcs.cpp`

- Lines/functions/headings: `resolveNpcSpawnCached` (around line 530).
- Old content or behavior: search at most 24 safe-standing candidates before falling back to the raw spawn point.
- New content or behavior: search up to 128 candidates while retaining the existing 25 ms time bound.
- Reason: live events showed most Payload NPC raw spawn points classified inside solids and returned unchanged.
- Why unrelated behavior is preserved: the change only applies to spawn resolution and does not raise the per-call time cap.

# Issues filed

- `issue-87d8c9c7ffb1`: Payload cart is not collidable in Arcade mode (active fix task).
- `issue-1d1b9c62ac08`: Payload Arcade NPCs fall through the world (active attempted fix; runtime proof pending).
- `issue-cc2bab947abc`: stable and legacy moving-object carry modes.
- `issue-4714c46d6136`: configurable pulsing through-wall cart outline.
- `issue-0767dde510b1`: smooth interpolation of moving-object transforms.
- `issue-067bf728d818`: investigate Payload rate below 60 Hz.
- `issue-9b5be6c88de0`: AI access to live journals for existing runs.
- `issue-df9cc9acbb0d`: low-CPU/headless Payload reproduction path.

# Changed files and reverse steps

| File | Before | After | Why | Reverse |
|---|---|---|---|---|
| `src/physics/physics-types.h` | Static-only `SweepHit` identity | Adds entity ID and surface velocity | Carry moving-box contact identity | Restore this session's two fields only |
| `src/physics/movement/physics-collision-glb-sweep-slide.cpp` | Arcade sweep checked static world triangles only | Adds bounded dynamic box sweep and support tracking | Cart contact and moving surface response | Revert this session's hunk only; preserve concurrent edits |
| `src/network/server-npcs.cpp` | Spawn safe search cap 24 | Candidate cap 128, same 25 ms limit | Try more safe nearby spawn candidates | Restore cap/comment only |
| Eight issue records | Absent | Added in `docs/issues/bug/` and `docs/issues/capability/` | Preserve report and route follow-up work | Close/supersede through issue workflow; retain append-only records |

# Contract / ABI changes

- Contract: internal `SweepHit` gains entity contact metadata.
- Version bump: none.
- Compatibility: no packet or persisted schema changed; build is pending.
- Reverse: remove the two fields and dynamic sweep response if later evidence rejects the design.

# State / schema changes

- Domain: issue tracker only.
- Old schema: unchanged.
- New schema: eight fully specified issue records.
- Migration: not applicable.
- Reverse: issue workflow only; records remain append-only.

# Deleted paths

- Path: none.
- Before: not applicable.
- Why: not applicable.
- Reverse: not applicable.

# Diagnostics

- Owner/category: `doGLBSweepSlide`, Collision.
- Input: Arcade capsule, physical box bounds, cached entity world triangles, entity velocity.
- Decision: sweep only actor-colliding physical boxes whose transformed bounds overlap the capsule's swept AABB.
- Output: entity-tagged sweep hit, moving-relative velocity response, support metadata, and `collision.arcade_entity.sweep`.
- Failure or rejection reason: non-box or non-actor-colliding physical entities are skipped; no live contact event has yet been observed.
- Rate limiting: new structured event is limited to one per thread per second.

# Validation

- Focused skill paths and results: routed collision, movement, Payload, NPC, issue-intake, dev-loop, and proof documents were read; no external skill applied.
- Tests and exact commands: no tests added or run, per the runtime-first repository rule.
- Build status: generation 421 (`game-dll-20261121T150703129448Z-23d2a4`) failed after 749.063 s: four compiler translation units reported out-of-memory allocation failures. Its dev-loop event confirms build note `99fcbee87ab4` was attached. Generations 422 (`game-dll-20261121T151936891977Z-a6abf2`) and 423 (`game-dll-20261121T152531204381Z-544470`) remained active at the final check (2026-11-21T15:53:42Z). Generation 422 compiled the edited `physics-collision-glb-sweep-slide.cpp` and `server-npcs.cpp` units; generation 423 had completed 413 translation units. Neither attempt had published a DLL at final check.
- Runtime or hot-reload evidence: existing run `20261121_144350` was active at initial inspection, but later dev-loop status reported no active run. The session did not stop it. No exact new DLL identity or post-change contact event is available; behavior is unproven.
- Output files: `logs/2026-11-21/20261121_144350/server-01/events-0001.jsonl`; `.dev/build-timings/game-dll-20261121T150703129448Z-23d2a4.jsonl`; `.dev/build-timings/game-dll-20261121T151936891977Z-a6abf2.jsonl`; `.dev/build-timings/game-dll-20261121T152531204381Z-544470.jsonl`.

# Measured evidence

- Before values: screenshot HUD displayed `SERVER RX: 42.9 Hz`, `ACTUAL: n/a`, `TICK: 8444`; existing journal recorded NPC collision-world `headless_triangles=31270`, `npc_triangles=31270`, `solid_ids_aligned=true`, and fall samples with `candidate_count=0`, `sweep_hits=0`, `world_contact=false`. Spawn events classified many raw spawn points inside solids and returned them unchanged (`corrected_m=0`).
- After values: no live post-change measurements collected.
- Timestamps: first automatic DLL attempt failed at 2026-11-21T15:19:35Z; build timing records capture subsequent attempts.
- Tick/frame/network measurements: no post-change measurements. HUD receive cadence is not treated as server simulation rate.

# Regression review

- Regression entry appended: no.
- Why this is or is not a confirmed regression: a reported bug and a proposed repair are not a confirmed regression; no runtime acceptance is available.
- Related regression paths: `docs/regressions/collisions/README.md`; broader Payload investigation `issue-cf9cd77eb056`.

# Human acceptance

- Visual review: not performed.
- Gameplay review: not performed.
- Multiplayer review: not performed.
- Still unverified: cart contact, stable support on the cart, and NPC grounding in a running Arcade Payload game.

# Agent handoff context

- Owner: payload-arcade-collision-fix.
- Focus: observe the final DLL queue result and prove both behaviors in a Payload Arcade run.
- Files to check: the three source paths above; the attempt timing records; live server `events.jsonl`.
- What another agent should not redo: issue intake, current collision-source tracing, and registration of proof tasks `task-proof-7d953b610754` (cart contact) and `task-proof-25a21cfca3c4` (NPC grounding).
- Next exact step: inspect dev-loop status and timing records; if the DLL succeeds, use only the dev-loop runtime/test path and inspect exact run journals. Keep both behavior issues open until proof or an explicit reasoned exemption exists.

# Automatic evidence

- Branch/base/final: `checkpoint/one-checkout-condensation-20261111` / `d651e1332fccff70bc5cc4601dc24d55784820b5` / same base commit.
- Git status: shared checkout contains unrelated concurrent modifications; no clean-tree claim.
- Diff stat: the three source paths changed by this session show 208 insertions and 19 deletions.
- Build identity: attempt 421 failed OOM; attempts 422 and 423 were active at final check.
- Generation: 421 failed; 422 and 423 have no published result yet.
- Artifact: none published for this change at final check.
- Journal path: `logs/2026-11-21/20261121_144350/server-01/events-0001.jsonl`.
- Session ID: `work-f0fe497a9427`.

# How to reverse

- Uncommitted source file: revert only this session's hunks; do not restore the whole shared file.
- Deleted file: none.
- Committed change: not committed.
- Generated file: issue/changelog records are intentional; do not delete them to reverse behavior.
- ABI/state/schema change: internal `SweepHit` only; no persisted state or network ABI changed.
- Reverse only files touched by this session. Preserve pre-existing changes.

# Related feature record

- Feature path: `docs/issues/bug/20261121-payload-cart-is-not-collidable-in-arcade-mode-issue-87d8c9c7ffb1.md`; `docs/issues/bug/20261121-payload-arcade-npcs-fall-through-the-world-issue-1d1b9c62ac08.md`.
