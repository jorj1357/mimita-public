# issue-4d9eb4b1a8ad: Wait over budget: exe-build at compiling

## Human report (verbatim, immutable)

> exe-build wait at compiling exceeded the wait budget


## Review (not-wedged-noop-build-overhead)

### 2026-11-22T04:22:13.391Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Review of wait exe-build:10:2026-11-22T04:03:03.139Z (elapsed 61.087s, budget 60.0s).

VERDICT: not-wedged / expected no-op EXE-build fixed overhead. The trip fired 1.087s past the flat 60s budget while a no-op EXE build was in its pre-cutover window; the build published successfully about two minutes later. Root cause is the same flat-budget policy already reviewed on sibling issue-f92715ba3c7f (queue portion of the same request b2290797) and on issue-60f01796d952. This entry records this occurrence's exact evidence; no duplicate fix or new issue created.

EVIDENCE (run 20261122_011031, journal logs/2026-11-22/20261122_011031/dev-loop/events.jsonl):
- L529: request 2026-11-22T03:52:50.598Z-b2290797 accepted 03:52:53.167Z from codex-ingame-gui-reconnect; this is the same request behind sibling queue issue issue-f92715ba3c7f.
- L581: dev-loop.wait.over_budget 04:04:25.319Z: wait_id exe-build:10:2026-11-22T04:03:03.139Z, kind exe-build, stage compiling, reason "compiling 0/45 [CXX] src\\renderer\\renderer.cpp", elapsed_seconds 61.087 >= budget 60.0, started_utc 04:03:03.139Z, build_id 2026-11-22T02:37:06.972018Z, generation 10, source_generation 11, request_ids ["2026-11-22T03:52:50.598Z-b2290797"], created=true (= this issue).
- L580: dev-loop.wait.reviewer-wake records wake.queued=true for this wait.
- L582-L584: dev-loop.exe-cutover 04:04:27.068Z (exe_sources=46 linked_game_sources=0), dev-loop.cap-render 04:05:14.981Z (game_sources=643 raw_gl_sources=0), dev-loop.build.completed 04:05:15.090Z (build 2026-11-22T02:37:06.972018Z, source_generation 10, duration_seconds 66.81, error empty). The wait began 04:03:03 and the build published 04:05:15 (~132s) with no error.
- build/progress.json (written 04:04:00.047Z by pid 11356): result "noop", compiled 0, skipped 87, elapsed_s 43.014, phase build_complete, phase_seconds compile_s 0.255, link_reason "current". The "compiling 0/45 ... renderer.cpp" stage/reason therefore did not describe the actual work: no TU was compiled, and most of the wait window is fixed setup plus (after the crossing) the cutover and CAP_RENDER gates.
- The reused EXE artifact's .dev/builds/2026-11-22T02-37-06.972018Z/build-identity.json still names source_generation 1, while build.completed claims source_generation 10, so the artifact identity under-reports the generation it is stamped with.

WAKE TRACE (second divergence: attribution, not delivery):
- The correct wake was queued exactly once: wake-19f1e3a36126, agent build-wait-reviewer, issue issue-4d9eb4b1a8ad, queued 04:04:06.243Z (.dev/agent-wake/events.jsonl L1173), claimed and launched through the codex adapter 04:04:24 (L1174-L1175). That adapter attempt did not complete; the scheduler stale-recovered it at 04:06:08 (L1181) and relaunched through the opencode adapter at 04:11:51 (L1184-L1185), which is this session.
- However, dev-loop journal L580 wake.launch_result embeds the previous occurrence's prompt, log, and wake id (issue-f92715ba3c7f, wake-8ce6b21fbc58, log 2026-11-22T03-53-55.583Z). The dev-loop journal alone therefore cannot prove which wake belongs to this issue; only .dev/agent-wake/events.jsonl and requests/wake-19f1e3a36126.json do. Suggested fix: record the wake request id / prompt hash at queue time instead of a last-launch result.

CAUSE OF THE RECURRING FRICTION:
- dev-loop.py WAIT_ISSUE_SECONDS = 60 (dev-loop.py:109-110) applies unchanged to exe-build from active_build_started (dev-loop.py:3483-3501). No-op EXE builds still run setup + build.py (43s) + exe-cutover + cap-render, which is close to or above 60s, so healthy no-op EXE builds structurally trip this issue.
- docs/specs/dev-loop/change-to-evidence-latency.md:76-86 and 122-127 expect per-class budgets, a truthful stage/reason, and appended later observations. dev-loop.py:3537-3545 instead reports only the first crossing (elapsed stays 61.087s even though the wait ran ~132s), and the recorded stage/reason were stale.

CONFIDENCE: high for the causal chain, the no-op result, and the successful publish (line-referenced journal plus build/progress.json); medium as to whether the 60s exe-build budget is intended policy or an over-broad threshold.

RECOMMENDATION (human approval required; no files claimed or edited):
- Consolidate with issue-e044a931c54b (waiting is the enemy) and issue-136d4553ff92 (account for every build stage): give exe-build a measured per-class budget that accounts for documented no-op plus gate overhead, and/or gate issue creation on absence of forward progress.
- Record the EXE exe-cutover and CAP_RENDER gate stages in wait/build timing so "compiling" no longer covers post-compile gates.
- Append later observations for a still-waiting wait_id (the spec already requires it) and record the true active build identity rather than latest_build.

## Context

### 2026-11-22T04:22:35.933Z — build-wait-reviewer

related_build: 2026-11-22T02:37:06.972018Z
related_run: 20261122_011031

## Agent suggestion

### 2026-11-22T04:23:00.748Z — build-wait-reviewer

Wake attribution defect for this occurrence: dev-loop journal dev-loop.wait.reviewer-wake (events.jsonl L580) embeds the prior issue-f92715ba3c7f prompt/log and wake-8ce6b21fbc58 instead of the actual wake-19f1e3a36126 (queued 04:04:06.243Z, .dev/agent-wake/events.jsonl L1173). Fix: record the wake request id and prompt hash returned at queue time, not a last-launch result, so the dev-loop journal can prove which wake belongs to the issue. Also record EXE exe-cutover and CAP_RENDER gate stages: this no-op EXE build spent 43s in build.py (build/progress.json result=noop, compiled=0) plus about 74s of post-build gates before publishing 04:05:15, while the wait stage remained compiling. Consolidate the budget change under issue-e044a931c54b and issue-136d4553ff92.
