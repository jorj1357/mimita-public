# issue-87d8c9c7ffb1: Payload cart is not collidable in Arcade mode

## Human report (verbatim, immutable)

> cant collide with payload in the arcade collision mode


## Triage

### 2026-11-21T14:56:16.298Z — payload-arcade-collision-fix

Summary: Fix the Arcade capsule contact path so actors collide with Payload cart physical geometry; verify against the active Payload scene without disturbing the shared run.
Owner: payload-arcade-collision-fix
Priority: high

## Scheduled independent review

### 2026-11-21T20:39:03.109Z — cpp-live-generation-timers

A bounded independent review was queued as wake wake-40413bf735f4 after 166 completed work sessions (trigger=sync). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (unproven)

### 2026-11-21T20:46:25.601Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Independent bounded review 2026-11-21T20:4xZ (issue-review-sweeper). Scope: issue brief/readiness, proof status, linked artifacts, journals. No source edits, build, restart, or fix claimed.

OBSERVED FACTS
- Readiness: ready=true, score 8, state=triaged; one open task payload-cart-arcade-contact owned by payload-arcade-collision-fix (agent inactive since 2026-11-21T15:55:08Z). Issue has no linked build or run records (builds=[], runs=[]).
- Source fix is present in the working tree: src/physics/physics-types.h adds SweepHit entityId/surfaceVelocity; src/physics/movement/physics-collision-glb-sweep-slide.cpp adds a bounded dynamic physical-box candidate pass and emits collision.arcade_entity.sweep (around line 836); server-npcs.cpp spawn candidate cap 24->128. git diff --stat for the three files: 208 insertions, 19 deletions.
- Source hashes current vs published DLL snapshots: sweep-slide=6b5fd3cec1d3deab... and physics-types=f3f5ca679e7e59c3... match build snapshots for generations 428 (game-dll-20261121T193255143084Z-1b7951), 429 (194526...8036b9), 430 (195311...0a1309), 431 (200541...5b40e3), and 432 (201417...2d9df4). So the fix IS compiled into the current published DLL (build/current.json: generation 432, status ok, published 2026-11-21T20:17:09Z).
- Runtime evidence is absent: rg found zero 'collision.arcade_entity.sweep' occurrences in every file under logs/. The only extant run journal is 20261121_144350, and its server (PID 7556) loaded build\\game\\generation-420\\game-g420.dll (event game.host.probe.started, launch_id 2026-11-21T14:43:50.330Z) - pre-fix. players=0; last journal write 15:29:59Z.
- dev-loop --status: lifecycle=starting, build=published_stale, run=RUNNING (server PID 7556, uptime ~5h59m), latest_source_stale=True. The live server has not activated generations 428-432.
- Proof record task-proof-7d953b610754: result=unproven, status=runtime_proven, journal_path='', run_id='', game_generation=0, verified_utc=''. The board/status lists this task as unproven.

INFERENCE / UNCERTAINTY
- The fix is compiled and published but has never been exercised in a run pinned to a published post-fix generation. This is a proof/activation gap, not a source gap.
- The 'status=runtime_proven' with empty journal/run/generation and empty verified_utc is internally inconsistent; likely a recording/verification bookkeeping mismatch rather than a behavior claim. Exact semantics of 'status' vs 'result' not confirmed from this sweep.
- Static reading cannot establish that the dynamic box sweep actually blocks the capsule, that contact is stable, or that static-world Arcade behavior is unaffected; only a pinned run can.

CONTRARY EVIDENCE
- None found supporting success. The existing generation-420 payload.cart-trace events describe the pre-fix state (cart_entity=1 present; many NPCs world_contact=false) and cannot validate this change.
- Note for the user's 'live game should count' rule: the running game does not count here because its journal pins generation 420, not the fixed generation.

NEXT DISCRIMINATING CHECK
- Through the root dev-loop, launch (or restart only if the scenario requires) a Payload/Arcade run pinned to published generation 432 (or any of 428-432 with matching source hashes) and drive a player root capsule into cart entity id 1. In that exact run journal require: game.host.probe.started showing generation 428-432; at least one collision.arcade_entity.sweep with entity_id=1 and actor=<player>; no capsule/cart penetration across the contact; and unchanged static-world Arcade contact. If only an isolated test run is used, keep the shared run 20261121_144350 untouched and report the pinned artifact IDs separately. Keep the issue open until that journal exists or an explicit proof exemption is recorded.
