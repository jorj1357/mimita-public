# issue-8ccf9935c145: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (reviewed-no-fix)

### 2026-11-22T22:01:12.873Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewed against this exact wait's dev-loop build attempt and wake records.

Verdict: NOT a stall; the wait mechanism worked as designed and the build succeeded.

Evidence (direct observations):
- Wait flagged 2026-11-22T21:23:48.277Z, elapsed 60.043s (budget 60s, +0.043s). wait_id game-dll-build:2026-11-22T21:22:48.132Z.
- In-flight attempt: game-dll-20261122T212248194307Z-487aa2, generation 526, revision b6822def653afcf57a88c8ab357cee8f3f7c38862e309c261f8d7505cfaaa755, source_generation 18, request auto-18.
- It published successfully at 2026-11-22T21:28:06.662Z (generation-526 generation.json status=ok, total_seconds=318.267).
- Change was src/input/input-router.cpp + header src/input/input-router.h (25 dependents). 26 TUs compiled, 617 skipped. compile phase 205.575s, link 4.713s.
- changed_source_preflight alone was 88.594s (21:23:04.698Z -> 21:24:33.292Z), dominated by src/input/input-router.cpp at 88.474s. At the 60s flag the build was still inside preflight, so the flagged reason "[GAME BUILD] revision=..." was simply the last detail, not evidence of a hang.
- Slowest TUs: engine-tick-net.cpp 143.596s, engine-tick-camera.cpp 116.736s, engine-tick.cpp 116.149s, engine-tick-state.cpp 115.152s, pause-menu.cpp 109.789s. TU queue waits reached ~154.8s, indicating CPU/worker contention in addition to slow TUs.
- Exactly one issue and one reviewer wake for this occurrence: issue opened 21:23:48.279Z; wake wake-2eddc1138df5 queued 21:23:52.484Z; only one wake.queued event for this issue. Acceptance for the per-occurrence mechanism is met.

Root cause of the recurrence (high confidence): devscripts/dev-loop.py:3531-3547 flags game-dll-build on total elapsed since build start with a flat WAIT_ISSUE_SECONDS=60. A single changed TU here (input-router.cpp 88.5s) is already over 60s, so this class of edit produces a guaranteed false positive. This repeats the finding already recorded in issue-a236165fa6fc; the systemic fix belongs to issue-a236165fa6fc / issue-9a4503f6ed68 (owner wake-proof-wait), not a new issue.

Evidence-quality defect in the issue record (new, reproducible): for this game-dll-build wait the stored evidence carries the wrong IDs. evidence.build_id = 2026-11-22T19:53:22.116562Z is the latest EXE artifact (.dev/builds/2026-11-22T19-53-22.116562Z/build-identity.json, source_generation 4), because dev-loop.py:3543 uses self.latest_build (set from the EXE publish at dev-loop.py:3814). evidence.generation = 525 is the last *published* DLL generation, because game_build_generation is only assigned on publish (dev-loop.py:2975-2976); the in-flight generation/attempt was 526 / game-dll-20261122T212248194307Z-487aa2. related_build is empty. So the issue does not actually carry the in-flight build/generation IDs required by its acceptance, even though wait_id + started_utc recover the attempt from .dev/build-timings.

Scale/flood evidence: the coord board currently holds 123 open/in_review issues titled "Wait over budget: game-dll-build at game_build" and 158 open build-friction issues total. One issue per wait_id (build_friction.py:143-200) is the documented design, but at this build cost it floods the board and spends a reviewer wake per ordinary rebuild.

Recommended fix (not implemented here; needs human approval and a separate claim):
1. Make auto-wake budgets progress-aware or stage-aware (reset on phase/TU progress, or a >=180s / predicted per-TU budget for game-dll-build) instead of flat 60s total elapsed.
2. Record the in-flight attempt id and game_build_source_generation in the wait evidence (and set related_build), not latest EXE build / last published generation.
3. Group recurring identical game-dll-build-at-game_build waits into one rolling issue with occurrences instead of one issue per wait_id.
4. Reduce the 88.6s preflight/single-TU compile and the 100-143s engine-tick TU compiles under issue-136d4553ff92 / build-coordinator.

No source files were edited or claimed. Confidence: high on timings, wake count, and the ID mismatch (read directly from .dev/build-timings, build/game/generation-526, .dev/builds, .dev/agent-wake, and the coord store); medium on the relative contribution of slow TUs vs CPU contention to the 318s total.
