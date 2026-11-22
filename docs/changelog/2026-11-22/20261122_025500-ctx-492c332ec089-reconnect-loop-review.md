# ctx-492c332ec089 In-Game Reconnect Loop Review

# Task

- Task ID: issue-review-sweeper wake-7a485196e824
- Session ID: session-47da939ed660 / work-255a312e9f85
- Owner: issue-review-sweeper
- Focus: bounded independent review of high-priority user context ctx-492c332ec089 ("inside mimita exe thru the ingame gui ... reconnecting issue loop and then disconnected") and its linked issue-cf9cd77eb056
- Summary: Reviewed context routing/ownership, readiness, proof state, and the newest pinned server/client journals. Reproduced the full user sequence in isolated run 20261122_021421 and found the first divergence in the server NPC AI phase (one actor costing ~25.9s; npc phase 34.2s at 19 NPCs) rather than the reconnect protocol. Appended one evidence review (id 96, verdict `confirmed_reproduction`), acknowledged the context, messaged the issue owner and context owner, and suggested one bounded NPC-AI task. No source edit, build, run, stop, or fix claimed.
- Status: REVIEW_RECORDED_OPEN
- Date, time, timezone: 2026-11-22T02:55:00Z
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 07ba9003

# Pre-existing changes

- The shared checkout contains 74 pre-existing modified/untracked paths from other agents. This session edited no source and reverted nothing.
- Files created by this session: this changelog only. Coordination state (context ack, issue review id 96, issue discussion messages, issue suggestion) was written through `agent-coord.py`.

# Requested behavior

Review the new high-priority user context, confirm the linked issue owns it, verify readiness/ownership, inspect exact runtime evidence, and record the first divergence without editing code or committing.

# Findings

- Routing: ctx-492c332ec089 is linked primary to issue-cf9cd77eb056 by codex-ingame-gui-reconnect; readiness ready=true, score 8, missing=[], state in_review, 12 tasks. No ownership repair was needed.
- Reproduction: run 20261122_021421 (EXE 8b5838cb..., game generation 440, TDM/atdm, server PID 29824, client PID 2744) shows Connected 02:15:23.532Z, Reconnecting 02:24:16.727Z, Connected 02:24:56.200Z, Reconnecting 02:25:15.663Z, Connected 02:25:20.226Z, then server timeout.expired/reconnect.expired/player.removed at 02:27:55.787Z-.788Z (silence_ms=70028).
- False-positive label: every Reconnecting `network.session.health` entry carries `false_positive_suspected=true` (last_heard_age_ms 745 and 93 vs stale_threshold_ms 2500) because snapshot_age_ms crossed 10033/10006.
- First divergence: server snapshot production outage. In 021421, snapshot build/send occur at tick 22165 02:24:06.026Z and next at tick 22166 02:24:56.192Z; heartbeat gaps 52.9s and 14.5s while uptime_ms advanced normally.
- Shared FCS run 20261122_011031 has the same class worse: 11,168 `performance.server-tick-stall`, 2,803 loop windows below 30 Hz; worst tick 83132 total_ms=34337.396, npc_ms=34210.957 with npcs=19 and budget 10ms. `npc.tick-cost` at 02:32:51.387Z attributes ai_update_ms=27525.132 to worst_ai_actor=100000 (25853.461ms); logger_mutex wait/hold 0.0.
- Secondary defects: duplicate reconnect datagram processing (9 client reconnect.accepted in ticks 97921-97945; 5 server receive/lookup/accepted for one burst), and run.json argv carries two `--mode` values.

# Specification alignment

- Followed agent-coordination, issue-intake-and-triage, and agent-proof-and-verification expectations.
- Treated build identity, runtime journal evidence, and user-visible state as separate claims. No behavior completion claimed.

# Exact implementation changes

- None to source or config. Append-only review id 96 on issue-cf9cd77eb056; context ack for ctx-492c332ec089; evidence messages to root-connection-reliability and codex-ingame-gui-reconnect; one bounded task suggestion.

# Validation

- Build status: no build requested; no compiler, module worker, or second dev-loop invoked.
- Evidence read: `logs/2026-11-22/20261122_021421/{client-01,server-01,dev-loop}/events.jsonl`, `logs/2026-11-22/20261122_021421/run.json`, `logs/2026-11-22/20261122_011031/server-01/events*.jsonl`, `src/network/connection-health.cpp`, `src/network/multiplayer-packets.cpp`.
- Review artifact: issue-cf9cd77eb056 review id 96, verdict confirmed_reproduction.

# Measured evidence

- 021421: 49.5s without snapshot build/send; 52.9s heartbeat gap; uptime 550984 -> 603883ms across the gap; 557 client snapshots received vs 730 server snapshots sent; max applied-snapshot gap 50.162s.
- 011031: worst tick 83132 total_ms=34337.396, npc_ms=34210.957, ai_update_ms=27525.132 (actor 100000), 19 NPCs.
- Loop windows in 011031: 2,803 below 30 Hz, many 0.44-1.04 Hz.

# Regression review

- Regression entry appended: no. The reconnect loop is a downstream symptom; the primary defect is the server NPC AI stall, which the issue already tracks as an open behavior gap.

# Human acceptance

- Visual/gameplay review: not applicable; no HUD screenshot was captured and the shared run had no attached client during the review window.
- Still unverified: whether the false-positive Reconnecting label is acceptable when snapshots are starved, and the exact NPC AI sub-phase that consumes ~25.9s.

# Agent handoff context

- Owner: issue owner root-connection-reliability; context owner codex-ingame-gui-reconnect; NPC AI phase needs a dedicated owner.
- Files to check: `src/npc` AI update path around actor 100000 targeting/LOS/path, plus `src/network/connection-health.cpp` retry gating.
- Do not redo: this review, context routing, and the 021421/011031 journal correlation.
- Next exact step: at the next shared FCS stall, capture actor 100000 npc.tick-cost sub-phase timings and one CPU sample; fix that sub-phase before changing reconnect timing, then run a 20-cycle reconnect matrix with badconn off.

# How to reverse

- No source or build to reverse. Remove this changelog only through normal review if created in error; preserve append-only issue records and coordination messages.
