# issue-f8873319f12b: Upgrade the local AI player controller

## Human report (verbatim, immutable)

> my computer has a .exe of mimita being controlled by the ai NPC controller and it is stuck focusing on teammates, walking into corners, looking at npcs that are behind walls/it shouldnt know about, stuck in corners not mkaing meaningful progress, not dashing or doing any movement things etc, it needs tob e super upgraded


## Task linked

### 2026-11-20T04:59:51.417Z — runtime-60hz

Task: ai-target-visibility (step 0)

## Task linked

### 2026-11-20T04:59:52.620Z — runtime-60hz

Task: ai-navigation-movement (step 0)

## Context

### 2026-11-20T06:25:26.910Z — work-metrics

reproduction: Launch a local match with a player controlled by the in-game AI controller and observe target choice, navigation around walls, corner escape, and movement actions.
expected_behavior: The controller selects visible hostile targets, ignores unseen targets, escapes corners, makes progress, and uses jump/dash/weapon actions when appropriate.
acceptance: Live journal shows hostile visible target selection, no teammate focus, no wall-only tracking, and measurable progress with movement actions.
paths: ['src/actor/player-ai.cpp', 'src/actor/player-ai.h', 'docs/specs/ai/player-ai.md', 'docs/specs/dev-loop/agent-coordination.md']
evidence: {'runtime': 'logs/2026-11-20/20261120_040807', 'symptom': 'teammate focus, wall targets, corner sticking, no meaningful movement'}

## Context

### 2026-11-20T06:49:04.429Z — work-metrics

reproduction: Launch a local match with a player controlled by the in-game AI controller and observe target choice, navigation around walls, corner escape, and movement actions.
expected_behavior: The controller selects visible hostile targets, ignores unseen targets, rejects its own actor, escapes corners, makes progress, and uses jump/dash/weapon actions when appropriate.
acceptance: Live journal shows hostile visible target selection, no teammate focus, no wall-only tracking, and measurable progress with movement actions.
paths: ['src/actor/player-ai.cpp', 'src/actor/player-ai.h', 'docs/specs/ai/player-ai.md']
related_build: 397
related_run: 20261120_063800_01
evidence: {'build': 'generation-397', 'build_attempt': 'game-dll-20261120T063933641878Z-848734', 'compiled': 'src/actor/player-ai.cpp', 'runtime': 'pending; active server is generation 394'}
