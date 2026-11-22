# issue-ee4dac126d29: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-wait-policy-noise)

### 2026-11-22T02:59:14.412Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Bounded independent review by build-wait-reviewer, wake-85acdf2c323c, 2026-11-22. Read-only: no source edit, no build/test requested, no fix or closure claimed.

CONCLUSION
Not a hung build. The exact attempt behind this wait completed successfully as generation 441, 131.9s after it started and 71.9s after the 60s budget fired. The wait is expected noise from a flat 60s budget against a legitimate compile+link+publish job. Record completeness and one-wake-per-occurrence acceptance pass; the same evidence-attribution gaps seen on issue-565c90a0d817 remain.

VERIFIED WAIT OCCURRENCE
- wait_id game-dll-build:2026-11-22T02:29:36.695Z; kind game-dll-build; stage game_build; elapsed 60.01s; budget 60.0s; started_utc 02:29:36.695Z; request_ids ["auto-1"]; source_generation 1.
- Journal logs/2026-11-22/20261122_011031/dev-loop/events.jsonl has exactly one dev-loop.wait.over_budget for this wait_id (created=true, wall_time 02:30:44.759Z) and exactly one dev-loop.wait.reviewer-wake for the same wait_id (wall_time 02:30:44.758Z).
- Deadline ~02:30:36.7Z fell during the compile phase, while src/network/reliable-gameplay-events.cpp (27.696s) and 11 module TUs compiled. No stall or error event.

PINNED ATTEMPT EVIDENCE (.dev/build-timings/game-dll-20261122T022936769355Z-e0de53.json; build current.json/ready.json)
- attempt game-dll-20261122T022936769355Z-e0de53; started 02:29:36.769Z; completed 02:31:48.690Z; total_seconds 131.911; success true; no_op false; generation 441; revision d6c61abbdc1a9da82417db9162b6db3421ca903c9c4f854f5453e004f26df24d; code_hash c469fa8223ee9b2d3be143e52f84b702a7601fbc1dd2c2977b6e203aa3aa3cd2; dll build/game/generation-441/game-g441.dll; published ready 02:31:48.719Z.
- Phase durations (ms): changed_source_preflight 27736.16; object_cache_scan 1759.86; compile 17617.726 (worker compile_seconds 45.345); link 38515.801; publish 14492.243; coherence_validation 725.188; toolchain_setup 48.714.
- Dominant subphases: link.gnu_linker 38079.851ms; publish.external_dependency_manifest 12908.344ms; changed_source_preflight 27736ms. Worker total 88.068s; the balance is dev-loop-side snapshot/publish overhead.
- ~71.9s of legitimate work remained after the wait fired (02:30:36.7Z -> 02:31:48.690Z).

ACCEPTANCE CHECK
- wait_id, stage, elapsed, evidence: present (issue readiness ready=true, missing=[]).
- build/run IDs: partial. Evidence build_id is the latest EXE artifact 2026-11-21T02:26:50.306117Z, generation null, source_generation 1; the real attempt game-dll-20261122T022936769355Z-e0de53 and generation 441 appear only inside the reason string. First divergence: dev-loop.py:3532-3533 sets build_id=self.latest_build and generation=self.game_build_generation, and game_build_generation is only assigned after success (dev-loop.py:2967-2980), so an in-flight wait is labelled with the previous artifact.
- reason: weak. The reason is a mid-JSON fragment (":58330.375,\"build_attempt_id\":\"game-dll-...\",\"progress_write_ms\":2.358,...}). First divergence: _record_game_build_output (dev-loop.py:2829-2860) stores game_build_detail = line.rstrip()[-240:]; a long [GAME_FILE] JSON line yields a truncated tail, and check_wait_budget uses game_build_detail as reason (dev-loop.py:3528).
- One reviewer wake for this occurrence: yes, exactly one. .dev/agent-wake/events.jsonl: wake-85acdf2c323c queued 02:30:37.087Z, claimed/launched codex 02:30:44.175Z/391Z (pid 17096); codex exited; retry opencode 02:35:36.626Z; claimed/launched opencode 02:42:20.699Z/986Z (pid 3812) — ~11m44s delivery delay from adapter fallback.
- occurrences stays empty: the terminal result (131.911s, generation 441) is never attached. dev-loop.py:3539 suppresses repeats of the same wait_id via reported_wait_ids; build_friction.record_wait_incident can append via issue_occurrence (build_friction.py:164-169) but is never reached again.
- reviewer-wake launch_result captured text for a different wake (issue-565c90a0d817), because wake_reviewer records agent-wake.py run --once stdout (build_friction.py:244-251), which may serve another queued wake.

POLICY NOISE
- Budget is the flat WAIT_ISSUE_SECONDS=60s. Corpus .dev/build-timings/game-dll-*.json (172 samples): p50 136.1s, p75 377.5s, p90 1005.1s; 105/172 (61.0%) exceed 60s. For whole-game DLL builds the flat 60s budget is below the median, so recurrence is expected and is not per-occurrence evidence of a fault.
- Brain currently holds 78 build-friction issues, 67 "Wait over budget", 55 game-dll (states new=21, in_review=46). Dedupe is per exact wait_id (build_friction.py:143-169), so each distinct wait occurrence intentionally creates a new issue.

RECOMMENDATION (not claimed here)
- Systemic fix remains owned by issue-136d4553ff92 (build-coordinator: account for every DLL build stage) and issue-e044a931c54b (build-runtime-subagents: remove avoidable waits). Stage evidence added as a suggestion to issue-136d4553ff92: here link.gnu_linker 38.1s + publish.external_dependency_manifest 12.9s + changed_source_preflight 27.7s dominate, while worker wall was 88s.
- Attribution: record the in-flight attempt_id and target generation in wait evidence, append the terminal result as an occurrence instead of suppressing it, and record a human-readable phase line (not the last 240 chars of a JSON row) as reason.
- Do not lower or raise the 60s budget here; a measured per-class budget needs human approval.
- Keep in_review until attribution/occurrence evidence is fixed and verified in a journal.

Confidence: high (pinned attempt JSON, build current.json/ready.json, journal over_budget + reviewer-wake events, wake queue/claim/launch events, 172-sample timing corpus). No source, build, or runtime behavior changed.
