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

## Agent suggestion

### 2026-11-23T19:25:18.180Z — bloodtest-bodycam-runtime

2026-11-23 runtime observation: attempted documented screenshot_at_tick now bodycam-live against Bloodtest run 20261123_192224 through dev-loop --command; client returned terminal.command.unknown (terminal command not registered). Screenshot evidence is therefore unavailable by this route; bodycam visual acceptance remains unproven. Next check: implement/verify screenshot command surface described by docs/specs/dev-loop/visual-proof-screenshots.md, then capture identity-matched frame.

## Agent suggestion

### 2026-11-23T21:02:46.750Z — issue-review-sweeper

Process proposal from issue-review-sweeper (issue-503f3e410c01 sweep, 2026-11-23T21:0xZ). Evidence: across three reviews the Mace white active-pose clause has remained human-visual-only. The tint is already computed in src/combat/weapon-viewmodel.cpp:233-245 (active pose forces mTint=white) and src/combat/weapon-viewmodel.cpp:618-627 (uUseColor/uColor white), but it is only emitted by a printf gated behind DebugConfig::DEBUG_WEAPON_VIEWMODEL (:628-631). No StructuredLogger event for viewmodel tint exists in any 2026-11-22/2026-11-23 journal, so screenshot-at-tick cannot currently assert "white only during _active" from a journal predicate.

Proposal: extend this issue's screenshot-at-tick capability with one bounded, change-gated `weapon.viewmodel.tint` event emitted by WeaponViewModel::render when the pose-active boolean or uUseColor changes. Fields: actor_id, weapon_id, pose_action suffix, pose_active, use_color, tint rgb, tick. Rate-limit to transitions only (<= a few per attack) to avoid per-frame spam, and keep it behind the existing debug toggle if the owner prefers; the acceptance is that one replay journal proves white during `_active` and the configured tint immediately after.

Measurable target: for a Mace active-pose proof run, the journal contains one tint transition to (1,1,1) at an accepted active tick and one transition back to the configured mace tint within <=2 ticks of pose end, with zero tint-change events per frame (transition-only). Smallest experiment: add the transition event, run one scripted Mace swing in Arcade, and check the two corresponding events bracket the accepted melee.contact.claim.accepted tick. This replaces a human-only visual step with machine-readable proof and is independent of the mace damage/parity work.

No source edit, build, or run was performed by this review; owner assignment and scheduling are for human/root review.

## Agent suggestion

### 2026-11-24T05:09:27.754Z — opencode-deepseek-v4-1

Related defect filed as issue-130ee745052a. Evidence: in run 20261124_041854 agent.screenshot logs/.../bloodtest-crisp.bmp completed and wrote a 1920x1080 BMP, but the image was an unrelated Rocket League window. src/main-systems.cpp:215-290 uses GetDC(nullptr) and BitBlt at the host window rectangle, not a framebuffer readback, and does not detect hidden/covered/unfocused windows. The screenshot_at_tick contract should capture the post-UI client framebuffer and record headless/window state; a desktop fallback must return failed rather than an unrelated image.
