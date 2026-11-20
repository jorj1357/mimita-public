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
