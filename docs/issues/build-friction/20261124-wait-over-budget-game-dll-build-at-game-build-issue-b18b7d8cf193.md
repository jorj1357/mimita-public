# issue-b18b7d8cf193: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-live-with-open-defects)

### 2026-11-24T20:22:31.973Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Independent bounded review by build-wait-reviewer (wake-39e8a107ab28), read-only; no source edit, build, close, or fix performed.

OBSERVED FACTS
1. Genuine over-budget wait. Attempt game-dll-20261124T200649543312Z-74e74d, generation 613, source_generation 10, started 2026-11-24T20:06:49.543Z, total 107.111s (changed_source_preflight 17.24s, compile 77.056s, link 7.401s; 14 compiled / 631 skipped; changed path src/entities/player-render.cpp). The wait fired at 20:07:49.6Z (60.086s) while the build was mid-compile at 10 jobs; resource sampling shows host CPU mean 99.82% / peak 100%. The build was progressing, not stalled. Evidence: .dev/build-timings/game-dll-20261124T200649543312Z-74e74d.json.
2. Outcome. success=false, failed_phase=link. ld.lld undefined symbols include MimitaNet::mpLocalGameplaySimulationReady, mpRequestNpcSpawn, mpRequestTeleport, mpRequestExplode, mpSendShotEvent, mpSendPelletBlastRequest. This is a shared-batch object/source mismatch (consistent with root-mode3-latency msg-3b63539049f2, repeated compiler and link errors in shared attempts), independent of the wait hook and not caused by it.
3. Identity defect, violating this issue's own acceptance. evidence.build_id=2026-11-24T19:27:02.742428Z (previous published EXE) and evidence.generation=null, while the in-flight attempt was generation 613 / game-dll-20261124T200649543312Z-74e74d. Root: dev-loop.py lines 3896-3898 use self.latest_build and self.game_build_generation (assigned only on completion).
4. Reason defect. evidence.reason is a truncated GAME_FILE translation_unit.completed JSON tail (progress_write_ms 4.092, record_overhead_ms 4.799), not a readable stage summary. Root: dev-loop.py lines 3112/3142 detail=line.rstrip()[-240:], read at dev-loop.py line 3892.
5. Reviewer wake. wake-39e8a107ab28 created 2026-11-24T20:07:50.513Z, claimed/launched 20:12:42.193-232Z (~291.7s dispatch). Within the <=5 min target but near it.
6. Journal gap. No logs/2026-11-24 dev-loop events.jsonl contains this wait_id or attempt id; .dev/state.json events_path is null. The proof expectation's dev-loop wait event is therefore not durably present for this occurrence.

INFERENCE / UNCERTAINTY
- The flat 60s WAIT_ISSUE_SECONDS is below the normal cost of an incremental whole-game DLL rebuild (this attempt spent 77s compiling 14 TUs), so the wait fires on healthy progress; record_wait_incident keys on the per-attempt wait_id, so each such attempt mints a new issue + reviewer wake. Both are already tracked under issue-9a4503f6ed68 and are not re-filed here.
- The link failure is a separate defect owned by the batch/compiler owners; it is not the subject of this wait issue.
- I did not reproduce a literal same-wait duplicate; the noise is the aggregate per-attempt pattern.

CROSS-LINKS
- reason-field defect: issue-6e08d89b55e3 (fresh occurrence appended).
- flat budget / stale identity / per-wait duplication: issue-9a4503f6ed68.
- wake dispatch latency: issue-84c584957e65.
- missing journal: issue-0498cbef4ef3 (per prior reviews).

NEXT DISCRIMINATING CHECK
- On the next game-DLL attempt, compare that wait's evidence.build_id/generation to the run's dev-loop.game-build.started build_attempt_id/generation and confirm evidence.reason is a readable summary, not a JSON tail. Identity and reason fixes are confirmed only when those match.
