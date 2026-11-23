# issue-e45921a75b6e: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Context

### 2026-11-22T18:30:22.158Z — build-wait-reviewer

related_build: game-dll-20261122T172200384686Z-a8e690
related_run: 20261122_144609_01

## Review (reviewed-with-findings)

### 2026-11-22T18:37:39.410Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Bounded review of wait occurrence game-dll-build:2026-11-22T17:21:59.980Z (elapsed 60.171s, budget 60.0s).

VERDICT: Acceptance criteria are substantially met, but the wait was productive compile work, not a stall, and the evidence record has two defects.

EVIDENCE (direct observation)
1. Dev-loop journal logs/2026-11-22/20261122_144609_01/dev-loop/events.jsonl: dev-loop.wait.over_budget for issue-e45921a75b6e at wall_time 2026-11-22T17:23:12.345Z, wait_id game-dll-build:2026-11-22T17:21:59.980Z, source_generation=3, request_ids=["auto-3"], reason revision=3e6b6f53b9f10274d7d6306d5b395710ea7ef01022deaddab26099861d206a17.
2. Actual game-DLL attempt: game-dll-20261122T172200384686Z-a8e690 (.dev/build-timings/game-dll-20261122T172200384686Z-a8e690.jsonl). build.started 17:22:00.384Z; changed-source preflight compiled src/npc/npc.cpp for 65.647s (translation_unit.completed status=failed at 17:23:38.471Z); build.failed at 17:23:46.191Z in changed_source_preflight with errors: src/npc/npc.cpp:3837 and 3839 "'Debug' is not a member of 'StructuredLevel'".
3. The immediately preceding attempt game-dll-20261122T171723932165Z-86a612 already failed at 17:21:59.495Z compiling the same file (202.979s) with "'class Npc' has no member named 'hasLoggedDash'". Both auto-builds were triggered by successive partial edits to src/npc/npc.cpp (build note 2f12830432c6, agent cpu-resource-reduction-implementation, "Make repeated NPC dash diagnostics opt-in debug events").
4. Reviewer wake was queued exactly once and correctly: .dev/agent-wake/events.jsonl shows wake-29e7399a7c36 wake.queued 17:23:06.482Z, wake.claimed 17:23:09.569Z (codex), wake.launched 17:23:11.435Z. The codex launch terminated; the scheduler later recovered the stale launched request and relaunched via opencode (wake.retry 18:02:46Z, claimed 18:02:49Z, launched 18:02:50Z), which is this review session.

FIRST DIVERGENCE
The wait exceeded budget during a normal changed-source preflight compile because a single translation unit (src/npc/npc.cpp) takes ~65-203s to compile, longer than the fixed 60s wait budget. It was not an idle/hung build; the compile was actively progressing and then failed due to an incomplete source edit. Root cause of the failure belongs to the src/npc/npc.cpp editor, not the wait policy (also reported in shared evidence msg-49121ad1911c, "Uncommitted game_build_worker.py priority fix crashed 3 builds").

EVIDENCE DEFECTS (do not block this occurrence)
a. evidence.build_id "2026-11-22T08:00:31.376619Z" is the last successful EXE build id, not the game-DLL attempt; the real attempt id was only embedded inside the reason string. related_build/related_run were empty and have now been set to game-dll-20261122T172200384686Z-a8e690 / 20261122_144609_01.
b. dev-loop.wait.reviewer-wake for this wait_id carries a launch_result string from a prior wake (wake-f2fe02f8fdc3 / issue-125a65704fa8 / log 2026-11-22T16-34-13.523Z), so that dev-loop field does not describe this occurrence's launch. The authoritative .dev/agent-wake journal is correct.

DUPLICATE NOISE (needs a decision, no code change made)
This is one of many identical kind+stage issues (issue-9aeec1fbae9d, issue-f9ea4f30ad18, issue-34993e327e19, issue-cfe902b4b659, issue-21dba28546bc, issue-edbac5d40c37, issue-125a65704fa8). Each distinct wait_id creates a new issue, and any edit to a slow TU (src/npc/npc.cpp) legitimately exceeds 60s, so every such build files another duplicate. Suggested direction: group by kind+stage into one canonical issue with occurrences, or raise/condition the budget on compile progress rather than first-start age. This is a policy/ownership decision for build-friction; left unowned.

NO CODE FIX CLAIMED. No repository files edited. Local filesystem only.

## Agent suggestion

### 2026-11-22T18:41:05.243Z — build-wait-reviewer

Recurring "game-dll-build at game_build" over-budget issues are duplicates produced by a fixed 60s budget against single-translation-unit compiles of 65-203s (src/npc/npc.cpp observed at 65.647s and 202.979s). Every edit to a slow TU therefore files another near-identical issue. Recommend the build-friction owner choose one policy, then implement under a separate claim: (1) one canonical issue per kind+stage, appending each distinct wait_id as an occurrence; (2) measure the budget from last compile progress/heartbeat instead of first-start age; or (3) raise the budget above the slowest-TU compile time. Related duplicates: issue-e45921a75b6e, issue-34993e327e19, issue-cfe902b4b659, issue-21dba28546bc, issue-edbac5d40c37, issue-125a65704fa8, issue-9aeec1fbae9d. Also fix evidence.build_id to carry the game-DLL attempt id, and stop copying a prior wake's launch_result into dev-loop.wait.reviewer-wake. No code changed; requires owner/human decision.
