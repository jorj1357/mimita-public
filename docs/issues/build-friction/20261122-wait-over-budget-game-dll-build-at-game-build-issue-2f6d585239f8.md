# issue-2f6d585239f8: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-wait-policy-noise-recurrence)

### 2026-11-22T08:21:07.793Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Bounded independent review by build-wait-reviewer (wake-fad907b7a791), read-only: no source edits, no build, no run started.

FACTS
- Wait occurrence: wait_id game-dll-build:2026-11-22T07:54:10.149Z, kind game-dll-build, stage game_build, elapsed 60.127s vs budget 60.000s (0.127s / 0.21% over).
- Pinned attempt: .dev/build-timings/game-dll-20261122T075410208033Z-f3ff3f.json. Started 2026-11-22T07:54:10.208Z, completed 07:57:20.531Z, success=true, total_seconds=190.312, generation=476, source_generation=3, revision=5677872a917dd2c1551483236ebacde3a64eb4566bc09aed6b3f6aa46f51b951, changed_paths=[src/network/server-npcs.cpp], note=fun-cs-npc-objective remove-the-retired-one-shot-NPC-dash-log-reset-field.
- Why it took 190s: 29 objects were cache-invalidated because src/npc/npc.h was newer than their objects (header fanout), not a stall. compile_seconds=140.493, compile_max=41.654s, changed_source_preflight=28.493s, link_seconds=25.899s.
- Active progress past the trigger: 106 timeline events after 07:55:10 (e.g. file_output_handled for admin-commands.cpp 07:55:34.080Z, object-commands.cpp 07:56:10.126Z, player-commands.cpp 07:56:13.814Z) and build.completed 07:57:20.531Z. The wait crossed 60s while the build was compiling, about 32% into its total duration.
- Wake: wake-fad907b7a791 was queued once for this issue at 2026-11-22T07:55:11.875Z (.dev/agent-wake/events.jsonl). No second wake for this wait_id.
- Evidence-quality defect (recurrence): issue evidence records build_id=2026-11-22T02:37:06.972018Z (stale latest EXE build) and generation=null, while the actual in-flight attempt is game-dll-20261122T075410208033Z-f3ff3f / generation 476 (embedded only inside reason). The acceptance requires the occurrence build/run IDs, so this record does not carry them reliably.
- Flood measurement: 117 open build-friction issues returned now; 89 are title Wait over budget: game-dll-build at game_build (17 new, 72 in_review). Same systemic policy noise as issue-1052cb7f28c4 (prior occurrence 07:17:51, reviewed 08:02:42) and tracked at umbrella issue-9a4503f6ed68.

ROOT CAUSE
The flat 60s WAIT_ISSUE_SECONDS (dev-loop.py:109, applied at 3520-3536) is below the normal duration of a healthy incremental game-DLL build, so every ordinary build trips an issue plus reviewer wake. build_friction.record_wait_incident keys uniqueness on wait_id only (build_friction.py:143-202), so recurring identical waits fan out into one issue and wake per attempt. This contradicts issue-9a4503f6ed68 acceptance (a second scan does not duplicate the issue) and this issue own risk field (a noisy wait policy could create duplicate friction issues).

VERDICT
Confirmed benign, actively-progressing wait; confirmed flat-budget miscalibration; confirmed duplicate-issue/wake flood. No build or runtime defect is asserted, and no runtime_proven behavior proof is claimed or required for this tooling-policy finding.

RECOMMENDED FIX (report only; claim separately on issue-9a4503f6ed68)
1. Stage/workload-aware budget derived from build-timing history (for game-dll-build, at least p95 / ~180s; keep setup and queue stages tight).
2. Alert on absence of new attempt progress events for a bounded interval (stall), not raw elapsed, for build stages.
3. Dedupe by stable kind+stage signature with appended occurrences (and one rolling issue) instead of one issue+wake per wait_id; keep every occurrence.
4. Populate wait evidence from the in-flight attempt (attempt id, generation, revision, changed paths, slowest phase) instead of stale latest_successful_build_id / game_build_generation.

CONFIDENCE: high on the benign nature and the policy/flood cause (pinned attempt summary + timeline + wake journal); high that this occurrence was not a build failure.
