# issue-40243f99b16c: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (genuine-over-budget-not-stall;cause=slowest-TU(main-systems.cpp 61.635s)>60s-budget;build-succeeded-gen647)

### 2026-11-25T21:28:59.595Z — build-wait-reviewer

Reviewer: build-wait-reviewer

**Build-wait review — build-wait-reviewer, wake-69030afc0934, 2026-11-25T21:2xZ UTC.**
Read-only: no source edited, no build/test started, no fix claimed, no proof recorded.

**Verdict: genuine over-budget wait, NOT a stall/hang. The build succeeded and published.**

Observed facts (pinned artifacts)
- wait_id `game-dll-build:2026-11-25T21:08:23.776Z`, stage `game_build`, budget 60.0s, woke at elapsed **61.955s**.
- Build `game-dll-20261125T210823887519Z-42efb4`, generation **647**, revision `1b99644e686cf2fcc7b3c044a206339a3a0be64abbfcebc79a8cc78f9f1e9bce`.
- Worker result `success: true`, completed `2026-11-25T21:10:19.011Z`, `total_seconds 115.109` (compile 79.609s, link 17.64s). `ready.json` published gen 647 at `21:10:18.734Z`.
- Source: `.dev/build-timings/game-dll-20261125T210823887519Z-42efb4.json`; build metadata `.dev/build/current.json`.

First divergence / root cause (observed, not inferred)
- `compile_max_ms = 61635.236` → **`src/main-systems.cpp` alone compiled in 61.635s**, which by itself exceeds the 60s wait budget. Any build that recompiles `main-systems.cpp` is therefore guaranteed to trip the wait policy regardless of health.
- Trigger: a header change to `src/config/camera-config.h` (fanout 14) plus `src/camera.cpp` and `src/config/camera-config.cpp`. All 16 cache invalidations are `dependency newer than object: src/config/camera-config.h` (header fanout); 17 units recompiled, 628 skipped.
- Resource evidence in the same artifact: `mean_host_cpu_percent 97.56`, `peak_host_cpu_percent 100.0`, `jobs 10`. The build was CPU-saturated, so the compile wall time tracks the slowest single translation unit (~61.6s) rather than a scheduling defect.

Inference / uncertainty
- This is a budget-vs-observed-p100 mismatch (`WAIT_ISSUE_SECONDS=60`, dev-loop.py:114/3839) where the p100 single-TU compile time (61.6s) already exceeds the budget. It is not an avoidable daemon wait in this occurrence.
- The heavier structural fix (reducing `main-systems.cpp` cost or header fanout) is a source change and out of scope for this review; no human approval present. Not claimed.

Acceptance status: issue fields are complete (`issue readiness` missing=[]), so no field repair needed. This occurrence is attributed and evidence-linked. Left unowned for the umbrella tracker; no duplicate issue created (195 non-closed identical-title wait issues already exist).

Suggested smallest next check (owner: build/latency umbrella issue-7d3dcfd7433b)
1. Confirm the wait budget should be measured against the p100 affected-TU compile time, or that event creation should require the build to be non-progressing, so a known-slow TU does not file a friction issue on every header touch.
2. If the goal is lower latency, target `src/main-systems.cpp` compile cost and `camera-config.h` fanout rather than raising the budget.
