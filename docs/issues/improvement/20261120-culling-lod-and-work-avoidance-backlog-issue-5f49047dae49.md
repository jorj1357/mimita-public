# issue-5f49047dae49: Culling, LOD, and work-avoidance backlog

## Human report (verbatim, immutable)

> suggestions like , ai shouls suggest more ideas like that efficiency and performance and fps increasses, make it so that ai can sugegst these thigns and raise issues and explan new workflows or ideas and put them in shared ai brain


## Task linked

### 2026-11-20T05:41:47.381Z — runtime-60hz

Task: culling-visibility (step 0)

## Task linked

### 2026-11-20T05:41:49.498Z — runtime-60hz

Task: offscreen-simulation-lod (step 0)

## Task linked

### 2026-11-20T05:41:51.145Z — runtime-60hz

Task: ui-dirty-budget (step 0)

## Task linked

### 2026-11-20T05:41:52.693Z — runtime-60hz

Task: network-relevance (step 0)

## Task linked

### 2026-11-20T05:41:54.338Z — runtime-60hz

Task: performance-auto-proposals (step 0)

## Context

### 2026-11-20T06:25:32.428Z — work-metrics

reproduction: Run the 67-NPC scenario and inspect frustum/LOD culling, UI atomics, network rate limits, and ragdoll/particle work.
expected_behavior: Each stage uses a bounded owner and avoids work outside the visible/useful set while preserving 60 Hz gameplay.
acceptance: Backlog has owner, measurement, and proof for each item; implemented items reduce work without visual regressions.
paths: ['docs/specs/performance/culling-and-work-avoidance.md', 'src/engine/engine-tick-render.cpp', 'src/config/collision-lod-config.cpp']
evidence: {'spec': 'docs/specs/performance/culling-and-work-avoidance.md', 'runtime': 'logs/2026-11-20/20261120_040807'}
