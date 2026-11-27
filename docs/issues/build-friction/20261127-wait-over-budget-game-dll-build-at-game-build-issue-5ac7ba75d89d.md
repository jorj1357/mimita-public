# issue-5ac7ba75d89d: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-wait-policy-noise-and-dispatch-defect)

### 2026-11-27T20:34:15.787Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewed this wait occurrence against the pinned build attempt, the dev-loop journal, and the reviewer-wake queue. No source edits, no build/run requested.

WAIT OCCURRENCE (verified)
- wait_id game-dll-build:2026-11-27T18:16:21.696Z; started_utc 18:16:21.696Z; elapsed 61.618s; budget_seconds 60.0; stage game_build; kind game-dll-build
- issue evidence build_id 2026-11-27T17:45:19.700118Z, generation 801, source_generation 72, request_ids [build-request-2c32fa3aef5e]. The hosting attempt game-dll-20261127T181622271460Z-618464 failed before publish, so no generation was actually published.
- The hosting attempt is genuine pre-worker wall time, not a slow TU and not a hang: timeline .dev/build-timings/game-dll-20261127T181622271460Z-618464.jsonl shows build.started 18:16:23; manifest_snapshot 18:16:44 to 18:17:43 (~59s, header_digest ~26s); manifest_validation to 18:17:50; source_snapshot 18:17:52 to 18:18:20 (~28s); then codegen packet_registry_codegen.py started 18:18:26, failed 18:18:29; build.failed 18:18:30 with error "codegen check failed: packet_registry_codegen.py" (total 128.442s, compile 0, link 0). At the 60s mark the build was still mid-manifest_snapshot.
- Resource evidence (postmortem + .resource.json): mean host CPU 99.97%, peak 100.0%, minimum free memory 808.4 MB (847,683,584 B). A concurrent live runtime was active during sampling: server pid 19460 authoritative mode team_deathmatch with 12 NPCs (npc.server-phase-window 536.1 ms total over 44 ticks; ai_ms 482.4), client pid 24004 at 105.8 FPS with all 106 frames over the 4.17 ms budget (p95 11.1 ms). This is contention evidence; it does not by itself prove contention caused the >60s wait.

RECORD / WAKE (acceptance)
- Issue was created 18:17:31.992Z and is complete: wait_id, stage, reason, elapsed, build/generation/source_generation, request_ids, plus evidence.
- Wake was queued exactly once for this occurrence: wake-8516085c89b5 queued 18:17:44.578Z (one request file, one wake.queued event) tied to this issue. So "one issue per wait occurrence + exactly one queued wake" holds.

FIRST DIVERGENCE (owner + line)
- devscripts/build_friction.py:244-246 wraps "agent-wake.py run --once" in subprocess.run(..., timeout=30) without catching subprocess.TimeoutExpired. For this wait that call timed out. The journal has dev-loop.wait.issue-failed at 18:18:16.404Z: error "Command [... agent-wake.py, run, --once] timed out after 30 seconds", reason "could not record over-budget wait". The exception escapes before dev-loop.py:3916-3926, so this occurrence has NO dev-loop.wait.reviewer-wake and NO dev-loop.wait.over_budget event, only wait.issue-failed. The issue and the wake queue both succeeded, so the failure reason is misleading and this issue's proof_expectation ("Dev-loop wait event plus the issue record and reviewer wake record") is not fully met. wait_id was already added to reported_wait_ids at dev-loop.py:3910, so it is not retried.

WAKE DISPATCH LATENCY
- wake-8516085c89b5 timeline from .dev/agent-wake/events.jsonl: queued 18:17:44.578Z; codex claimed/launched 18:42:10.022/18:42:10.061Z pid 26692 (drained by a later wake ~24m later); wake.retry to opencode 18:47:45.504Z ("previous adapter process exited; trying fallback"); opencode claimed/launched 20:29:58.038/20:29:58.056Z pid 9480. Queue-to-final-launch ~7941s (~2h12m) with two wake.launched events for one wait. Same pattern as issue-84c584957e65 / issue-fdbac689adea; not a stalled build.

WAIT-POLICY NOISE
- devscripts/dev-loop.py:3885-3901 fires on wall-clock age alone with one global WAIT_ISSUE_SECONDS=60 (no stage/progress check); build_friction.py:131-169 keys uniqueness to wait_id, and wait_id embeds the per-attempt start time, so each >60s attempt mints a new issue. This occurrence is one of many duplicate "Wait over budget: game-dll-build at game_build" issues. Canonical defect is inside issue-9a4503f6ed68 (item 4); related issue-81d40f5f20b3.
- The same 30s run --once timeout recurred in this run (17:47:55, 18:18:16, 18:22:42, 18:34:04); this occurrence is the 18:18:16 one, already documented in issue-fdbac689adea.

ASSESSMENT / NEXT (report only)
- Not a hang or a compiler stall. Two open defects explain this occurrence: the flat/per-attempt wait policy (issue-9a4503f6ed68) and the TimeoutExpired handling + slow dispatch (issue-fdbac689adea). This issue adds a concrete occurrence and pins the missing wait-event proof.
- Suggested smallest safe owner fix: catch subprocess.TimeoutExpired around build_friction.py:244-246 and still write dev-loop.wait.reviewer-wake/over_budget (or make the launch asynchronous) so a slow adapter cannot drop durable wait events or mislabel the failure. Do not change the 60s policy here; that belongs to issue-9a4503f6ed68.
- The hosting build failed a codegen check (packet_registry_codegen.py) at 18:18:29; that is a separate build defect, not the wait. Not investigated here.
- Cross-links: issue-fdbac689adea, issue-9a4503f6ed68, issue-81d40f5f20b3, issue-84c584957e65, issue-6e2d7b91e7d9, issue-136d4553ff92.
- Confidence: high (pinned journal lines, build-timings summary/timeline, wake request + wake events). Files edited: none. Build/test requested: none. Unknowns: cause of the codegen failure and whether it relates to the concurrent edit churn; not covered by this review.
