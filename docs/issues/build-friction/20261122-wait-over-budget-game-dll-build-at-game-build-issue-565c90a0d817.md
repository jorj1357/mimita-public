# issue-565c90a0d817: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-wait-policy-noise)

### 2026-11-22T02:36:09.786Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Bounded independent review by build-wait-reviewer, wake-ed4b3fb12444, 2026-11-22. Read-only: no source edit, no build/test requested, no fix or closure claimed.

CONCLUSION
Not a hung build. The exact attempt behind this wait completed successfully as a no-op reuse of generation 440, 296.7s after it started. The wait fired on a flat 60s budget while a legitimate codegen stage was still running; two later stages alone exceeded 60s. Record completeness and one-wake-per-occurrence acceptance pass; two evidence-attribution gaps remain.

VERIFIED WAIT OCCURRENCE
- wait_id game-dll-build:2026-11-22T02:05:41.793Z; kind game-dll-build; stage game_build; elapsed 63.949s; budget 60.0s; started_utc 02:05:41.793Z; request_ids [2026-11-22T02:05:18.283Z-d162f9e9, 2026-11-22T02:05:48.661Z-0af8a79d].
- reason: codegen:packet_registry_codegen.py started 02:05:52.484Z, elapsed_ms 8179.074, build_attempt_id game-dll-20261122T020544305357Z-268306.
- Journal logs/2026-11-22/20261122_011031/dev-loop/events.jsonl has exactly one dev-loop.wait.over_budget for this wait_id (created=true, wall_time 02:06:52.078Z) and one dev-loop.wait.reviewer-wake for the same wait_id (wall_time 02:06:52.076Z).
- Deadline 02:06:41.793Z fell inside packet_registry_codegen (started 02:05:52.484Z, ended 02:07:06.841Z, duration 74.357s). No stall or error event.

PINNED ATTEMPT EVIDENCE (.dev/build-timings/game-dll-20261122T020544305357Z-268306.json; build current.json)
- attempt game-dll-20261122T020544305357Z-268306; started 02:05:44.305Z; completed 02:10:41.039Z; total_seconds 296.731; success true; no_op true; generation 440; code_hash c9099e1bd97c81f640df89170b2ac41dedb091aef4ba82495173221c55a1a51c.
- Stage durations from the journal: resident_state_codegen 4.961s; packet_registry_codegen 74.357s; manifest_snapshot 197.512s (source_count 1008); manifest_validation 0.161s; source_snapshot 0.005s (reused_generation 440).
- Worker time 1.222s; the remaining ~295s was snapshot/codegen/validation overhead. Roughly 3m59s of legitimate work remained after the wait fired (02:06:41.793Z -> 02:10:41.039Z).

ACCEPTANCE CHECK
- wait_id, stage, reason, elapsed, build/run IDs, evidence: present (issue readiness ready=true, missing=[]).
- One reviewer wake for this occurrence: yes, exactly one. .dev/agent-wake/events.jsonl: wake-ed4b3fb12444 queued 02:06:51.056Z, claimed by codex 02:06:51.785Z, launched 02:06:51.862Z; codex exited; retry opencode 02:10:05.913Z; claimed/launched by opencode 02:30:44.489Z/02:30:44.581Z (pid 19672). Delivery was ~24 min late due to the adapter fallback.
- Attribution gaps (first divergence):
  1. Evidence build_id is the latest EXE artifact 2026-11-21T02:26:50.306117Z, generation null, source_generation 0; the actual attempt game-dll-20261122T020544305357Z-268306 and generation 440 appear only inside the nested reason string. dev-loop.py:3532-3533 sets build_id to self.latest_build and generation to self.game_build_generation (assigned only after success, dev-loop.py:2967), so a wait is labelled with the previous artifact.
  2. occurrences stays empty: build_friction.record_wait_incident can append via issue_occurrence (build_friction.py:164-169), but dev-loop.py:3539 suppresses repeats of the same wait_id via reported_wait_ids. The terminal result (296.731s, generation 440) is never attached to this issue.
  3. The reviewer-wake launch_result captured in this wait's event describes a different queued wake (issue-17abc587c89a, wake-ff9869ed026f, log 2026-11-22T01-48-34.891Z-b...), because wake_reviewer records agent-wake.py run --once stdout, which may serve another queued wake.

POLICY NOISE
- Budget source is a flat 60s; the spec asks for measured per-class budgets (docs/specs/dev-loop/change-to-evidence-latency.md, Budgets and outliers: measured baselines; a human approves budget changes).
- Corpus .dev/build-timings/*.json, 171 samples with total_seconds: p50 131.9s, p75 375.9s, p90 1005.1s; 104/171 (60.8%) exceed 60s. For whole-game DLL builds the flat 60s budget is below the median, so recurrence is expected and is not per-occurrence evidence of a fault.
- Brain now holds 75 build-friction issues, 64 Wait over budget, 53 game-dll; 63 unowned, states new=19 / in_review=45. Dedupe is per exact wait_id (build_friction.py record_wait_incident), so each occurrence intentionally creates a new issue.

RECOMMENDATION (not claimed here)
- Systemic fix is owned by issue-136d4553ff92 (build-coordinator: account for every DLL build stage and speed) and issue-e044a931c54b (build-runtime-subagents: remove avoidable waits). Stage evidence added as a suggestion on issue-136d4553ff92: dominant stage here was manifest_snapshot 197.5s, then packet_registry_codegen 74.4s, while the worker did 1.2s of real work.
- Attribution: record the in-flight attempt_id and target generation (or parse them from the reason) in wait evidence; append the terminal result as an occurrence instead of suppressing it; attribute launch_result to the wake actually launched.
- Do not lower or raise the 60s budget here; a measured class budget needs human approval.
- Keep in_review until attribution/occurrence evidence is fixed and verified in a journal.

Confidence: high (pinned attempt JSON/current.json, journal over_budget and reviewer-wake events, wake events, 171-sample timing corpus). No source, build, or runtime behavior changed.
