# issue-004c549df353: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-wait-real;recurrence-of-issue-747933df08a6-cluster;budget+attribution-owned-by-issue-136d4553ff92)

### 2026-11-28T18:14:13.908Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Bounded review of this wait occurrence. Read-only for source and builds: no source edited, no build or test requested by this agent, no fix or close claimed. Evidence read: this issue record, the root dev-loop journal `logs/2026-11-28/20261128_171225/dev-loop/events.jsonl`, `.dev/build-timings/game-dll-20261128T174046257052Z-eb3d52.json` (+ `.jsonl`/`.resource.json`), the current `build/game/current.json`, and current `devscripts/dev-loop.py`.

WAIT OCCURRENCE (confirmed real, not a stall)
- wait_id `game-dll-build:2026-11-28T17:40:46.167Z`; started 17:40:46.167Z; elapsed at check 61.105s; budget 60.0s; reported stage `game_build`; request `build-request-e295f096a55b`.
- The waiting attempt is `game-dll-20261128T174046257052Z-eb3d52`, source_generation 80, revision `19e8d604f2833...`. It was a deliberate COLD object-cache rebuild: build note by `tdm-npc-focus-fire`, "Build self-repair runbook step 3: two consecutive link failures on different missing TUs", `expected_seconds=1000`, changed path `src/network/server-packet-handlers.cpp`.
- Progress was real, not hung: it eventually exceeded the worker's 1080s attempt timeout and failed at 17:58:52.311Z with `failed_phase=attempt_timeout`, `total_seconds=1086.062`, `worker_error="whole DLL build attempt exceeded 1080s during worker"`. So the 61s wait check landed roughly 1/17th into a declared ~1000s cold rebuild.
- Resource pressure was extreme: mean host CPU 99.13%, peak host CPU 100.0%, minimum available memory ~17.9 MB (18,763,776 bytes), max per-TU queue wait 1037.094s, 738 samples at 1s. The overage is a cold rebuild starved by full host CPU plus near-zero free memory, not a blocked wait.
- The build that finally published was a later attempt `game-dll-20261128T180034349425Z-3007f5` (generation 825, 131.735s) at 18:02:48Z. This wait's own attempt ended in timeout/failure.

ACCEPTANCE CHECK (partially unmet; gap already owned elsewhere)
- Present and correct: wait_id, stage, elapsed, budget, started_utc, source_generation, request_ids, evidence.
- Not met/incorrect: `reason` is the same corrupted 240-char tail of the last `[GAME_FILE]` event JSON (`_record_game_build_output` sets `game_build_detail = line.rstrip()[-240:]`, `devscripts/dev-loop.py:3130`; wait reads it at `devscripts/dev-loop.py:3912`). `build_id` is the stale EXE artifact `2026-11-27T17:45:19.700118Z` (from `self.latest_build`, line 3916) and `generation` is 820 (last published) while the in-flight attempt targets generation 824 - the attempt id `...-eb3d52` and gen 824 leak only through the corrupted reason string (source: `self.game_build_generation` is assigned only on success, line 3917). `related_build` and `related_run` are empty.
- This reason/attribution defect and the flat-budget problem are recorded in `issue-136d4553ff92` (build-coordinator; see entries dated 2026-11-21 including the explicit `WAIT_ISSUE_SECONDS=60` + per-attempt wait-id note). I am not creating a duplicate issue for them.

REVIEWER WAKE (meets "exactly once per occurrence")
- Journal has exactly one `dev-loop.wait.over_budget` and one `dev-loop.wait.reviewer-wake` for this wait_id, both at 17:41:56.067Z; `created=true`, `wake.queued=true`, issue `issue-004c549df353`. No duplicate wake for this occurrence.

RECURRENCE (added here; no duplicate issue created by me)
This occurrence is recurrence #3 of the ~10-minute cluster already documented in `issue-747933df08a6` (which lists waits at 17:30:46, 17:34:21, and this 17:40:46). Four over-budget `game_build` waits occurred in today's run `20261128_171225`, each a different attempt, each its own issue+wake:
1. 17:31:56 -> issue-747933df08a6, elapsed 60.939s (attempt ba5757).
2. 17:35:26 -> issue-6ed1b2897c5b, elapsed 60.808s (attempt 3b08fd).
3. 17:41:56 -> issue-004c549df353, elapsed 61.105s (attempt eb3d52, this one).
4. 18:12:24 -> issue-0e64599cd133, elapsed 60.051s (generation 825 era).
This realizes this issue's stated risk ("A noisy wait policy could create duplicate friction issues").

BUDGET EVIDENCE (smallest actionable point)
- `WAIT_ISSUE_SECONDS` is a flat default 60s shared by every wait kind (`devscripts/dev-loop.py:114-115`), and the game-DLL branch fires solely on wall-clock age (`devscripts/dev-loop.py:3904-3920`). It does not consult the build note's `expected_seconds` nor recent stage durations. Here the agent's own build note declared `expected_seconds=1000`, so a 60s page was guaranteed and uninformative. A stage/class-aware budget (or inactivity/no-stage-progress based trigger) keyed on the build note expected duration would avoid this wake while still flagging a genuinely stuck build.

ASSESSMENT / OWNERSHIP / NEXT
- Wait detection is correct for this occurrence; the defect is attribution/reason/budget, owned by issue-136d4553ff92. Smallest next check there: carry the active attempt id, target generation, and build-note `expected_seconds` into wait evidence as structured fields; derive a readable reason from the parsed event payload instead of the 240-char raw tail; set related_build; and use a stage/class-aware budget so a declared cold rebuild does not page a reviewer every retry.
- Immediate cluster cause is not wait policy: `tdm-npc-focus-fire` owns the in-flight edits and the cache-quarantine cold rebuild under host starvation. That is their work, not a wait-policy defect.
- Confidence: high. Claims are pinned to the attempt JSON/JSONL, the run journal wait/wake events, and current source lines. No runtime behavior proof is relevant to this tooling issue.
