# issue-13d080318e1c: Automatically trace every client frame below 240 FPS and create or update diagnosis issues

## Human report (verbatim, immutable)

> also make a issue so that every time FPS dips  to like &#x20; 
> bewlow 240fps  or 
> add a issue that maeks it so we auto trac and auto trace auto make a issue  or extend a existing issue whenever a mimtia exe client fps is below 240fps ever, for waht reasn, over time we needt o expand and make it so super like, near impossible  to misunderstand what is making the dslodown happen, we need that much information and mkae it so its ez for ai  to understnaad, relates to teh ai getting info ramtion  issue it hink, need to maek it so AI  view of mimita is muhc more clear and it knows everthing that is going on all the time


## Scheduled independent review

### 2026-11-24T13:14:00.175Z — root-mode3-latency

A bounded independent review was queued as wake wake-48eb3936273d after 558 completed work sessions (trigger=sync). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (needs-evidence)

### 2026-11-24T13:28:13.594Z — issue-review-sweeper

Reviewer: issue-review-sweeper

### 2026-11-24T13:2xZ — issue-review-sweeper (bounded independent review, wake-48eb3936273d)

Verdict: needs-evidence for the automated capability. This review does not edit source, build, run a new scenario, close the issue, or claim a fix.

**Readiness and proof status (observed at 13:15Z).** `issue readiness` returns ready=true, score 8, state new, zero tasks, owner empty. `proof status` shows no builds, runs, or proof records for issue-13d080318e1c, and the issue has no linked reviewer records before this one. Linked artifacts are the performance specs, `devscripts/agent_coord.py`, and `src/engine/engine-tick-render.cpp`; the canonical journals are the per-process `events.jsonl` segments under `logs/<date>/<run>/client-01/`.

**Observed facts: partial instrumentation already exists, but not the requested automation.**
- `src/perf/perf-frame.cpp:928-1004` emits `performance.frame-budget` when `total_ms > 4.0`, rate-limited to at most one record per second. In run `20261124_122748`, client-01 (PID 34756, launch `2026-11-24T12:27:48.487Z`), the current segment contains 194 frame-budget/fps records. Example at t=3076.889: `total_ms=5.4249`, `budget_ms=4.0`, `over_ms=1.4249`, top five self scopes; the event envelope carries `run_id`, `launch_id`, `pid`, `process`, and `instance_id`. This is bounded per-miss evidence, but it is a per-second summary of the current frame, not an explicit before/after frame window.
- `src/perf/perf.cpp:1499-1532` emits the always-on 1 Hz `performance.fps` with `frame_budget_ms=4.166667` and `frames_over_budget`. Same run: t=2981.47, 124/171 frames over budget, fps 169.9, p99 14.56 ms; t=2987.48, 69/85 over budget, max 130.6 ms. The frame count is therefore visible.
- `src/perf/perf-frame.cpp:391-464` emits a 60-frame `performance.resource-window` with main-thread CPU, private/pagefile bytes, actor/effect/visible counts, and per-scope totals. In this run `gpu_timer_queries_enabled=false` and `gpu_frame_samples=0`, so GPU timing and display refresh are still unavailable.
- `src/perf/perf.cpp:1125-1159` emits one `performance.baseline` per process with `build_id`, `process_id`, target FPS, and vsync. For this run it is in the rotated segment `client-01/events-0002.jsonl` at t=8.26 with `build_id=2026-11-24T07:48:35.684015Z:52c3ec0a...`, `target_frame_ms=1.001` (maxFrames=999), and `vsync=false`. Its own fields `run_id` and `events_path` are empty even though the outer envelope has `run_id`; the identity is split from current evidence by journal rotation.
- No code bridge exists from a frame miss to an issue. A search of `devscripts` for `frame-budget`, `frames_over_budget`, and `240` finds only `perf-evidence.py:172` (sums the count) and `build_resource_sampler.py:659` (field names). `agent_coord.py:2810` has `issue_occurrence`, and `build_friction.py:166` uses it for exact wait matches, but nothing calls it for a frame signature. There is no stable frame-slowdown signature, no new-issue branch, no extend-existing-issue branch, and no occurrence history for this feature.

**Inference and uncertainty.** The instrumentation is real, so the issue is not "no capability at all"; it is partial observability plus a missing automation/signature layer. I infer the no-automation finding from source and journal absence, not from a negative runtime experiment. I did not rerun a controlled workload or cause a distinct signature. It is also unclear whether the three different budgets are intentional: `1.001` ms baseline target (`maxFrames=999`), `4.0` ms frame-budget gate, and `4.1667` ms fps/resource-window gate. `docs/specs/performance/performance.md:65-80,278` sets the client gate at 4.167 ms; the `performance.frame-budget` reason string says "240fps budget" while logging frames from 4.0 ms. A 4.0-4.1667 ms frame would be labelled over the 240 FPS budget even though the spec says at-or-under 4.167 passes. That may be a deliberate stricter logging threshold, but it is currently undocumented and makes the miss count ambiguous. Likewise, I cannot tell whether the empty `run_id`/`events_path` in `performance.baseline` is a placeholder bug or deliberate.

**Contrary evidence.** The issue's "count and identify every miss" and "bounded context" clauses are closer to satisfied than its evidence metadata implies: per-window over-budget counts, per-miss top scopes, identity envelope fields, main-thread CPU, resource counters, and bounded 60-frame summaries are already live. The acceptance clause "automatically create one issue with a stable signature and extend it without losing occurrence history" is the clause with no supporting evidence.

**Next discriminating check (owner, after creating a task).** Use one pinned client run and one additional run with a deliberately different dominant scope; first join `performance.baseline.build_id` across all rotated segments to the frame-miss envelope, then prove the smallest bridge: derive a signature from mode/map plus dominant scope names and stable ratios, append one occurrence to a single issue for the same signature, and create a second issue for the distinct signature. Record exact run/launch/PID/build, threshold definition, evidence range, occurrence count, and log bytes/overhead. Also decide one budget source of truth (4.0 logging vs 4.167 acceptance) and record display refresh plus whether GPU timing was available, or mark each as an explicit observability gap. Until that run exists, issue creation alone must not be treated as proof of FPS or root cause.
