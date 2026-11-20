# issue-549680e53b26: Frame consistency: no frame below 240 FPS

## Human report (verbatim, immutable)

> i want to get like 240fps 99.99% of the time ... no frames should be below that if we can help it ... super super consistent no jittering that is biggest issue no stuttering or jittering


## Task linked

### 2026-11-20T04:59:57.219Z — runtime-60hz

Task: ui-cost-trace (step 0)

## Task linked

### 2026-11-20T04:59:58.859Z — runtime-60hz

Task: frame-spike-trace (step 0)

## Context

### 2026-11-20T06:25:31.325Z — work-metrics

reproduction: Run the 67-NPC scenario with frame timing enabled and inspect accepted frame times.
expected_behavior: Every accepted frame is at most 4.167 ms (240 FPS), with no 239 FPS pass; 999 FPS is aspirational and not required.
acceptance: Journal reports 100 percent of accepted frames at or below 4.167 ms; visible 240 tested on a 240 Hz display.
paths: ['docs/specs/performance/performance.md', 'devscripts/dev-loop.py', 'src/engine/engine-tick-render.cpp']
evidence: {'runtime': 'logs/2026-11-20/20261120_040807', 'observed': '90-120 FPS, MAX spike 2368.59 ms', 'display_hz': 60}
