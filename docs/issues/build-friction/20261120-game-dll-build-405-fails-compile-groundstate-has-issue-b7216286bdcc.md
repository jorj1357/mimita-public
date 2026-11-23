# issue-b7216286bdcc: Game DLL build 405 fails compile: GroundState has no member groundNormal

## Human report (verbatim, immutable)

> src/network/server-gamemode.cpp:3949:52: error: 'const struct GroundState' has no member named 'groundNormal'


## Scheduled independent review

### 2026-11-23T13:52:31.486Z — resource-efficiency-goal

A bounded independent review was queued as wake wake-a03a672774ff after 406 completed work sessions (trigger=sync). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (independent-review: source clean at HEAD; later build 546 compiled TU success=true; proof expectation appears satisfied but no proof record or closure yet)

### 2026-11-23T13:59:47.481Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Independent bounded review by issue-review-sweeper (wake wake-a03a672774ff, sweep issue-review-sweep-406). No source edit, build, test, proof-registration, or closure performed by the reviewer.

OBSERVED (load-bearing)
1. Readiness: ready=true, score 8, missing=[], state=new, owner empty, tasks=0; no prior issue_reviews. No proof record references this issue (proof status/list scanned for b7216 / groundNormal / server-gamemode: none).
2. Original failure artifact is present: .dev/build-timings/game-dll-20261120T163826804025Z-9b84b5.json -> success=false, failed_phase=compile, total_seconds=1466.52, worker_error=compile failed: 1 file(s) in 336.59s. worker_errors[0].output shows src/network/server-gamemode.cpp:3949-3951: const struct GroundState has no member named groundNormal, plus the nlohmann push_back error at 3936. changed_paths were [src/actor/player-ai.cpp, src/mapeditor/map-editor-projects.cpp]; server-gamemode.cpp was rebuilt as a dependency and failed.
3. Current source at HEAD 0abd6231: SimpleMatch search of src/network/server-gamemode.cpp for groundNormal returns ZERO matches. The payload telemetry now reads valid GroundState members (grounded=ground.onGround, stable_grounded=ground.stableOnGround, world_contact=ground.hasWorldContact) plus collision.supportEntityId. src/entities/player.h:410-423 GroundState still has no groundNormal, consistent with removing the invalid usage rather than adding the field.
4. A later successful build proves the TU compiles: .dev/build-timings/game-dll-20261122T235222465818Z-9bfbf7.json -> success=true, generation=546, revision ac1ddd46..., total_seconds=267.197, worker_errors=0; its only changed_path is src/network/server-gamemode.cpp; the sibling .jsonl shows translation_unit.completed path=src/network/server-gamemode.cpp status=compiled seconds=151.088. This matches the issue proof_expectation (build-timing summary success=true for the fixed revision plus the corrected compile line).
5. Current build generation 560 (game-dll-20261123T132647317966Z-638ff7.json and build/current.json) is success=true and published, but server-gamemode.cpp is NOT in its changed_paths (cache-skipped), so g560 does not independently re-prove this file.

INFERENCE / UNCERTAINTY (inferred)
- The offending groundNormal line appears to have existed only in an uncommitted working-tree snapshot: git log -S ground_normal -- src/network/server-gamemode.cpp returns nothing, and the issue revision 4195db3b... is a 64-hex source-tree content hash, not a git commit (git cat-file: Not a valid object name). This looks like a dirty-tree compile error, not a committed regression.
- The exact commit that removed the reference cannot be pinned via git -S; the file is clean at least by commit 45580410 (2026-11-22 19:25) and compiled at generation 546 (2026-11-22 23:52). Exact removal point is unknown.

CONTRARY / CAVEAT
- This evidence is build-only, as the issue itself states; no runtime behavior applies. No proof task is registered, so proof state is untracked rather than proven. The freshest build (g560) skipped the file.

NEXT DISCRIMINATING CHECK (owner/human; not performed by reviewer)
- Register a proof task pinned to game-dll-20261122T235222465818Z-9bfbf7.json (success=true, server-gamemode.cpp status=compiled, worker_errors=0) and have the build-friction owner confirm closure. If stronger evidence is required, request one root-dev-loop build after a trivial server-gamemode.cpp touch so the TU recompiles at current HEAD; accept only success=true with 0 worker_errors.

Process note: one specific development-process proposal based on this sweep is filed on issue-91e3384b219b.
