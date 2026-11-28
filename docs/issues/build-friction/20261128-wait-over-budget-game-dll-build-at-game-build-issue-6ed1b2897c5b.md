# issue-6ed1b2897c5b: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Context

### 2026-11-28T17:53:21.175Z — build-wait-reviewer

related_build: game-dll-20261128T173421264252Z-3b08fd

## Review (reviewed)

### 2026-11-28T17:54:23.442Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewer: build-wait-reviewer (wake-6fde8872c1a3). Scope: review only; no source edit, no build requested.

OBSERVED (direct evidence)
- Wait record: game-dll-build:2026-11-28T17:34:21.168Z, stage game_build, elapsed_seconds=60.808 vs budget_seconds=60.0. The dev-loop journal logs/2026-11-28/20261128_171225/dev-loop/events.jsonl contains BOTH dev-loop.wait.over_budget (line 136) and dev-loop.wait.reviewer-wake (line 135), wall_time 2026-11-28T17:35:26.281Z. So issue record + dev-loop wait event + one queued reviewer wake are all present; this is not the journal-loss variant (issue-6e2d7b91e7d9).
- In-flight attempt game-dll-20261128T173421264252Z-3b08fd (source_generation 78, changed src/network/server-npcs.cpp) started 17:34:21.264Z, was still compiling at the 60.808s trigger, and completed 17:35:38.903Z (total 77.641s). Real work, not a stall. compile 65.735s dominated by server-gamemode.cpp 50.275s, map-editor.cpp 38.553s, server-packets.cpp 30.921s, server-packet-chat.cpp 29.332s, vehicle-networking.cpp 27.992s; link 4.094s. Resource pressure on the attempt: mean host CPU 84.74%, peak 100.0%, minimum available memory 163.8 MB.
- The attempt did not publish: success=false, failed_phase=link, undefined symbol MimitaNet::handleSpyKnifeHitClaim referenced by src/network/server-packets.cpp:2799/2804. That symbol IS defined at src/network/server-packet-handlers.cpp:1548 (present at HEAD:1542 too), yet server-packet-handlers.cpp was not recompiled in this attempt (absent from cache_invalidations and compiled list; several other TUs report "object file is empty"). This matches the recorded cache fault docs/regressions/dev-loop/game-dll-link-loses-translation-units-REG-2026-11-27.md, not a missing source definition.
- reason is again an unreadable mid-JSON tail: ":58250.0,\"build_attempt_id\":\"game-dll-20261128T173421264252Z-3b08fd\",...". Same defect as issue-6e08d89b55e3 (dev-loop.py detail = line.rstrip()[-240:]).
- Identity in the wait evidence is stale: generation=820 and build_id=2026-11-27T17:45:19.700118Z, while the in-flight attempt is generation 822 (progress tail) with source_generation 78. generation is only assigned on completion and build_id is self.latest_build (prior EXE build). Same defect as issue-9a4503f6ed68. Correct fields: source_generation=78 and request_ids=[build-request-b3c1804785d9].
- Reviewer wake: wake-6fde8872c1a3 queued exactly once at 17:35:24.330Z; codex claimed/launched 17:35:26.086/26.176Z (pid 21068) then exited; wake.retry 17:41:55.769Z switched to opencode; opencode claimed/launched 17:42:46.163/46.187Z (pid 24200). Queue-to-final-launch = 441.857s (~7m21.9s), attempts=2, two wake.launched events for one wait_id. The dev-loop.wait.reviewer-wake row is stamped with this wait_id/issue but embeds wake-7f80ca3ba980 (issue-136d4553ff92), not wake-6fde8872c1a3 (launch_result tail, build_friction.py:250). Same defect as issue-84c584957e65.

INFERENCE / UNCERTAINTY
- The 60s flat budget was crossed because this was a large team-brain.h header-fanout recompile under host CPU/memory pressure; the dominant cause is compile time, not the wait hook. The link failure is a separate cache fault that also made the attempt unhealthy. I did NOT verify whether the following retries (game-dll-20261128T173737563597Z-6b0a85, 17:37:37; game-dll-20261128T174046257052Z-eb3d52, 17:40:46) quarantined/cleared the stale object, so that remains unknown.

CONTRARY EVIDENCE
- Exactly one issue exists for this wait_id (no duplicate wait issue), and the wake was queued exactly once. The acceptance clause "a reviewer wake is queued exactly once" held; the one-launch intent did not (two wake.launched rows).

NEXT DISCRIMINATING CHECK
- Confirm the object cache for flags_identity b10e3a31b3ae8637 was quarantined/ rebuilt so MimitaNet::handleSpyKnifeHitClaim links on the next generation (per the REG-2026-11-27 self-repair runbook), and make build_friction.wake_reviewer return the just-queued wake id instead of the oldest drained request tail.
