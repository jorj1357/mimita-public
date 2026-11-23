# issue-3875f423f9bf: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (pass-with-evidence-gap)

### 2026-11-22T22:17:12.369Z — build-wait-reviewer

Reviewer: build-wait-reviewer

**Independent bounded build-wait review — issue-3875f423f9bf, agent build-wait-reviewer, wake-def06f3a23fd, 2026-11-22T~22:12Z.**
Read-only: no source edited, no compiler/build/worker/second loop invoked, no fix claimed, no file lease taken.

**Verdict:** pass-with-evidence-gap — the wait was real, one issue + exactly one reviewer wake were produced; the issue evidence omits/inaccurately labels the in-flight DLL build attempt id.

**Wait verified**
- Issue wait_id `game-dll-build:2026-11-22T21:53:51.481Z`; budget 60.0s; elapsed 60.094s; created 21:54:51.819Z.
- Exact attempt: `game-dll-20261122T215351509781Z-dd7c11` (started 21:53:51.509Z; +28ms vs wait started_utc). `.dev/build-timings/game-dll-20261122T215351509781Z-dd7c11.json`: success=true, generation 530, revision 778a07cc…, total_seconds 98.887, worker_total_seconds 82.764.
- First divergence (latency, not a stall): `translation_unit.completed` for `src/network/server-npcs.cpp` at 21:55:15.993Z (68.268s); `build.completed` at 21:55:30.405Z, i.e. 39.4s after the 60.1s trip. The budget was exceeded by a real, still-running compile, not a phantom wait.

**Cost breakdown (this attempt)**
- `src/network/server-npcs.cpp` compile: 68.268s (dominant; one TU alone exceeds the 60s budget).
- module_slot_wait: 13.012s before the first TU (build/module contention).
- link: 10.238s; object_cache_scan: 1.641s; publish: 0.586s; identity-provider compile ~0.98s.
- Build note: "Fix FCS threat-score helper scope blocking candidate DLL build" (`src/network/server-npcs.cpp`).

**Auto-intake + wake verified**
- Exactly one wake request for this occurrence: `wake-def06f3a23fd`, event `WAKE_e01a770e3de64b4c89c1d46e90346bc2`, queued 21:54:53.377Z (`.dev/agent-wake/events.jsonl`). No duplicate wake.
- Dedupe is by wait_id (`devscripts/build_friction.py:131-140,164-169`), so one issue is correct for this wait_id.

**Evidence gap (known; cross-linked, no duplicate issue created)**
- Evidence `build_id` = `2026-11-22T21:32:48.195707Z:e24aaf16…` (state `latest_successful_build_id`, the prior successful build) and `generation`=null. It is not the in-flight attempt `game-dll-20261122T215351509781Z-dd7c11`. `DevLoop.check_wait_budget` sets game-dll `build_id` from `self.latest_build` (`devscripts/dev-loop.py:3543`). The attempt joins only via `started_utc`. This same gap is already appended to owner issue-136d4553ff92 by earlier build-wait reviews; I did not create a duplicate suggestion.
- I could not locate a durable `dev-loop.wait.over_budget` / `dev-loop.wait.reviewer-wake` journal line for this wait_id under `logs/**/dev-loop/events.jsonl` or the root-loop logs; only the wake-queue record and this issue prove creation. Independently recording these events is covered by issue-136d4553ff92 / issue-9a4503f6ed68.

**Recurrence / noise (owners: issue-9a4503f6ed68 for policy, issue-136d4553ff92 for latency)**
- By design each distinct wait_id becomes a new build-friction issue (`build_friction.py:143-152`). Current on-disk count: 125 `game-dll-build at game_build` wait docs + 22 `exe-build` wait docs. The issue's own risk ("a noisy wait policy could create duplicate friction issues") is occurring; the sustained cause is that a common single TU (`src/network/server-npcs.cpp`) compile alone exceeds the 60s budget. No new issue filed.

**No code fix claimed.** Reducing the server-npcs.cpp compile or making the budget progress/attempt-class aware is owned work on issue-136d4553ff92 / issue-9a4503f6ed68 and needs human approval before shared build changes.

**Confidence:** high for the wait reality, cost breakdown, and single-wake dedupe; medium-high for the build_id→attempt join (started_utc only).
