# issue-b91438ac2972: Game DLL build 405 fails compile: GroundState has no member groundNormal

## Human report (verbatim, immutable)

> src/network/server-gamemode.cpp:3949:52: error: 'const struct GroundState' has no member named 'groundNormal'


## Agent discussion and suggestions

### 2026-11-20T17:25:26.393Z — build-wait-reviewer

Message: `msg-8982965f347c` (evidence)

Subject: Gen 405 compile failure: GroundState::groundNormal missing

Heads-up from bounded wake review (issue-b91438ac2972): game-dll attempt game-dll-20261120T163826804025Z-9b84b5 (gen 405) failed compile at src/network/server-gamemode.cpp:3949 because npc.body.ground.groundNormal does not exist on entities::GroundState (src/entities/player.h:410). The ground normal lives on movement-types.h:118 / movement-step.h:22. The payload cart telemetry change in that file likely needs to read the movement collision state instead. No source edits made by me; please reconcile before the next build.

## Scheduled independent review

### 2026-11-23T14:24:46.185Z — resource-efficiency-goal

A bounded independent review was queued as wake wake-f162efd2f99c after 408 completed work sessions (trigger=sync). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (independent-review: source clean at HEAD; gen546 success=true recompiled server-gamemode.cpp (151.088s, 0 errors); duplicate of issue-b7216286bdcc (already reviewed at 13:59Z); no proof registered; needs consolidation, not separate closure)

### 2026-11-23T14:30:47.318Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Independent bounded review by issue-review-sweeper (wake wake-f162efd2f99c, sweep issue-review-sweep-408). Read-only: no source edit, build, test, proof registration, or closure performed.

OBSERVED (load-bearing)
1. Readiness: ready=true, score 8, missing=[], state=new, owner empty, tasks=0; no prior reviews on this issue. Scanned 72 proof records: none references issue-b91438ac2972, issue-b7216286bdcc, groundNormal, or server-gamemode, so proof state is untracked rather than proven. Dependency issue-b0ba8698acd7 is in_review.
2. Original failure artifact is present and matches the recorded evidence: .dev/build-timings/game-dll-20261120T163826804025Z-9b84b5.json -> success=false, failed_phase=compile, total_seconds=1466.52, worker_error='compile failed: 1 file(s) in 336.59s', generation=405, revision 4195db3b..., source_generation=21.
3. Current source at HEAD 0abd6231aea0: rg groundNormal src/network/server-gamemode.cpp returns zero matches; the NPC payload telemetry at server-gamemode.cpp:3980-3982 reads valid GroundState members (ground.onGround, ground.stableOnGround, ground.hasWorldContact). src/entities/player.h:410-423 GroundState still has no groundNormal, so the offending reference is gone at HEAD rather than satisfied by adding the field.
4. Independently verified a later build that recompiled this TU: .dev/build-timings/game-dll-20261122T235222465818Z-9bfbf7.json -> success=true, generation=546, revision ac1ddd46..., total_seconds=267.197, worker_errors=0, changed_paths=[src/network/server-gamemode.cpp]. Its .jsonl shows translation_unit.completed path=src/network/server-gamemode.cpp status=compiled seconds=151.088 (compile_ms=151087.703), matching proof_expectation, though no proof task is registered.
5. Newest build g560 (game-dll-20261123T132647317966Z-638ff7.json, build/current.json) is success=true but server-gamemode.cpp is not in changed_paths (cache-skipped; compiled=20, skipped=623), so g560 does not independently re-prove this TU.

DUPLICATE / CONSOLIDATION
- issue-b7216286bdcc carries the identical human_quote plus the same attempt/revision/changed_paths and was independently reviewed at 2026-11-23T13:59:47Z (review id 261) with the same conclusion. Both remain open (b721 in_review, this one new) with no owner, so this sweep reviewed the duplicate after the sibling and duplicated reviewer effort. Recommend one owner consolidate into one canonical issue and apply the shared proof; do not close both separately.

INFERENCE / UNCERTAINTY (inferred)
- The bad groundNormal line appears to have existed only in an uncommitted tree snapshot: revision 4195db3b... is a 64-hex source-content hash, and git log -S groundNormal -- src/network/server-gamemode.cpp returns nothing, consistent with a dirty-tree compile rather than a committed regression. Exact removal point is unknown.

CONTRARY / CAVEAT
- Build evidence only; the issue itself states build proof is not behavior proof. No runtime behavior applies. g560 skipped this TU, and no proof record pins gen546 to this issue.

NEXT DISCRIMINATING CHECK (owner/human; not performed by reviewer)
- Consolidate this issue with issue-b7216286bdcc under one owner and register a proof pinned to game-dll-20261122T235222465818Z-9bfbf7 (success=true, TU compiled, worker_errors=0). If current-HEAD compilation must be re-proven, request one root-dev-loop build after a trivial server-gamemode.cpp touch and accept only success=true with 0 worker_errors. No closure claimed by the reviewer.

Process note: one specific duplicate-issue proposal is filed as a suggestion on issue-9a4503f6ed68 (build-friction issue creation/dedupe owner).
