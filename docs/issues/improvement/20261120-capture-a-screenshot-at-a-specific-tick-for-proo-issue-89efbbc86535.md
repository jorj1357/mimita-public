# issue-89efbbc86535: Capture a screenshot at a specific tick for proof

## Human report (verbatim, immutable)

> we should be able to get like ai agents to take screenshot at a specific tick to prove a specific thing is happenign


## Task linked

### 2026-11-20T04:59:55.537Z — runtime-60hz

Task: screenshot-at-tick (step 0)

## Context

### 2026-11-20T06:25:30.301Z — work-metrics

reproduction: Call screenshot_at_tick <tick> [name] during a proof run and inspect the produced PNG and sidecar.
expected_behavior: A PNG captured at the requested simulation tick plus a truthful sidecar identity; missed means missed, never a false success.
acceptance: PNG and sidecar exist at the requested tick; sidecar matches build/generation/run; journal records the command result.
paths: ['docs/specs/dev-loop/visual-proof-screenshots.md', 'devscripts/dev-loop.py', 'docs/issues/improvement/20261120-capture-a-screenshot-at-a-specific-tick-for-proo-issue-89efbbc86535.md']
evidence: {'spec': 'docs/specs/dev-loop/visual-proof-screenshots.md', 'state': 'not implemented'}

## Scheduled independent review

### 2026-11-22T06:42:03.914Z — cpp-latency-pressure-profile

A bounded independent review was queued as wake wake-aa92eb12662f after 255 completed work sessions (trigger=work_end). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (not-implemented-unproven)

### 2026-11-22T06:54:20.256Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Reviewer: issue-review-sweeper

Observed facts (read-only):
- Readiness (`python devscripts/agent-coord.py issue readiness issue-89efbbc86535`): ready=false, score 6/8, missing proof_expectation and risk. State=new, owner=unassigned. The single task `screenshot-at-tick` has empty acceptance/evidence/method. `proof status` contains no record for task_id `screenshot-at-tick`, so there is no unit, integration, or runtime proof for this issue.
- The spec is complete and demand is clear: docs/specs/dev-loop/visual-proof-screenshots.md:10-82 requires `screenshot_at_tick <tick> [name]`, a PNG plus JSON sidecar under `logs/<date>/<run-id>/client-01/screenshots/`, `result: captured|missed|failed`, build/generation/tick/camera identity, and framebuffer readback after the render/UI pass.
- The command is not implemented. `rg -n screenshot_at_tick` over the working tree matches only docs/specs/dev-loop/visual-proof-screenshots.md, docs/specs/dev-loop/scenario-input-recording.md:719, and issue/changelog prose; there is no match in src/ or devscripts/. The only registered visual command is `agent.screenshot` at src/main-systems.cpp:954-973.
- The existing capability is materially different: src/main-systems.cpp:198-295 `captureAgentWindowBmp` requires a caller-supplied relative `.bmp` path under logs/ (lines 203, 213), reads the window rectangle, and copies desktop pixels with `GetDC(nullptr)` + `BitBlt` (lines 251-257). It writes BMP only, has no PNG, no sidecar JSON, no requested-tick handling, and no truthful `missed` result. Its event carries only path/width/height/captured (lines 967-971).
- Runtime confirmation of the existing shape: logs/2026-11-22/20261122_052434/client-01/events-0054.jsonl:20585-20589. The agent command `agent.screenshot .../browser-current.bmp` was host-read at client tick 462 (line 20585), resolved by the terminal at `client_tick=66288` (line 20587), and `agent.screenshot.saved` reported captured=true 1024x768 (line 20589). The saved event itself records `tick=0` and no captured-tick or build/generation identity. logs/2026-11-22/20261122_024031/client-01/events.jsonl:54250-54254 shows the same shape at client_tick=45310.
- Those BMPs are desktop composites keyed to the window rectangle, not framebuffer readback: an occluded, minimized, or overlapped window can put foreign pixels into the image while the run identity is attached by position only.

Inference and uncertainty:
- The issue is genuinely not implemented at its acceptance contract. The current `agent.screenshot` cannot produce a PNG + sidecar at a requested tick, cannot record a truthful `missed`, and does not bind the image to build/generation/tick.
- Uncertainty is limited to runtime absence: I did not launch an isolated run this sweep, so absence is inferred from the registry source plus two journals rather than an observed unknown-command event.
- Tick identity is partially available already: the queued-command path records `client_tick` at terminal execution (66288, 45310), and the command carries a request/correlation ID. A future implementation can anchor the exact tick from that path, but the current saved event drops the tick (`tick=0`), which is the first missing step in the input-to-output chain.

Contrary evidence:
- This is not blocked by networking, build, or a missing specification; spec and command-queue tick delivery both exist. So the gap is implementation, not environment.
- A window-level visual capability is proven in use (two journals), so the issue is narrower than no screenshots existing: the missing pieces are the tick contract, PNG, identity sidecar, and missed-result truthfulness.

Next discriminating check:
- On a future isolated pinned run, enqueue `screenshot_at_tick 120 shot` and inspect that exact run journal for (a) `terminal.command.received` for `screenshot_at_tick` or an explicit unknown-command result, and (b) presence or absence of `logs/<date>/<run-id>/client-01/screenshots/120-shot.png` and `.json`. This distinguishes not-registered from registered-but-not-reached. The static check already shows no registration, so confidence in absence is high; the runtime check only closes a hidden-registry or alias uncertainty.

Confidence: high that acceptance is unmet and that no proof exists; medium on the exact owner path for a fix (terminal registration in src/main-systems.cpp plus the existing render framebuffer readback owner) because I did not open the render readback path this sweep.

Verdict: not-implemented, unproven; issue kept open. No source edit, build, or close performed.
