# issue-4ab3b491ad6c: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (reviewed)

### 2026-11-26T00:03:55.764Z — build-wait-reviewer

Reviewer: build-wait-reviewer

**Bounded review - build-wait-reviewer, wake-80862c08c2b5, 2026-11-25T23:5xZ UTC.** Read-only: no source edited, no build/run started, no fix claimed.

**Wait occurrence evidence (verified)**
- run `20261125_221046`, `dev-loop/events.jsonl`: `dev-loop.wait.over_budget` @ 2026-11-25T23:05:26.716Z (exact payload matches this issue) and `dev-loop.wait.reviewer-wake` @ 23:05:26.712Z.
- Build `game-dll-20261125T230423811208Z-c638b8` (revision 0222d3508a3e, source_generation 25) targeted generation 655, NOT 654 and NOT a hang.
- `dev-loop.game-build.result` @ 23:07:16: status=ok, compiled=167, skipped=478, compile=166.406s, link=1.047s, total=172.516s, worker=170.657s, failed_phase empty, errors [].
- So the 60.399s wait was a legitimate large mixed/header-fanout rebuild (vehicle-types.h fanout + packet schema regen), not a wedged worker. The budget trip is expected while the build is still compiling.

**Acceptance checklist (issue-4ab3b491ad6c)**
- wait_id present: PASS (`game-dll-build:2026-11-25T23:04:23.764Z`).
- stage present: PASS (`game_build`).
- elapsed present: PASS (60.399s; budget 60s).
- evidence present: PASS (embedded wait payload recorded).
- reason readable: FAIL - stored value is a mid-JSON tail, not a stage sentence. Root cause `devscripts/dev-loop.py:_record_game_build_output` sets `game_build_detail = line.rstrip()[-240:]`, slicing the last worker line's JSON. Already tracked by **issue-6e08d89b55e3**; this occurrence appended there, no duplicate filed.
- build/run identity: PARTIAL - `build_id` is the prior published id and `generation`=654 while the in-flight target was 655 (and the later occurrence recorded generation null). Root cause `check_wait_budget` reads `game_build_generation`, which is only set on success (dev-loop.py:3259). Tracked with the wait-stage measurement work in **issue-7d3dcfd7433b**.
- reviewer wake exactly once: PASS - `agent-wake/events.jsonl` has one `wake.queued` for wake-80862c08c2b5 @ 23:05:25.577Z; the two launches (codex 23:05:26, opencode 23:51:29 after `wake.retry`) are the known adapter-fallback path tracked by **issue-84c584957e65**, not a duplicate queue.

**New direct resource observation (not previously noted for this wait)**
`.dev/build-timings/game-dll-20261125T230423811208Z-c638b8.resource.json` (status captured, 153 samples, 1s): mean_host_cpu_percent=92.591, peak=100.0, minimum_available_memory_bytes=6,520,832 (~6.2 MB). Other concurrent-host builds in the same sweep reported 3.4-4.3 GB free, so this build ran near memory exhaustion. Direct observation only: `unavailable_fields` still lists matched_prebuild_resource_baseline and host_commit_charge_and_limit, so memory pressure as a *cause* of the compile duration is an unproven inference, not established. Appended as a gap occurrence to **issue-2a5c5da53565**.

**Recurrence**
A second game-dll-build over-budget wait 46 min later (`game-dll-build:2026-11-25T23:50:11.848Z`, generation 657, 68.915s, build total 129.5s) already has its own auto-issue **issue-6d572fc03e97** with the same truncated-reason and null-generation fields. Same root workload class (header-driven DLL rebuild), so this is a recurring pattern rather than a one-off; per-wait_id auto-issue policy is producing many near-duplicate `Wait over budget: game-dll-build at game_build` issues. That dedupe/policy question is a human decision (already listed as this issue's risk), not implemented here.

**Verdict:** Mechanism works as designed for this occurrence (one issue, one wake-queued). Two evidence-quality defects remain and are already owned elsewhere (issue-6e08d89b55e3, issue-7d3dcfd7433b). No code change made; no behavior completion claimed.
