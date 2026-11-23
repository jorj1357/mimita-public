# issue-c481ab36713a: Dev-loop must separate launch requests from live MiMITA process metadata

## Human report (verbatim, immutable)

> dev loop launch emtadata need to be separate from running metadata, get live  data from any mimtia process that is running and use that as the datae u read, add that as an issue


## Context

### 2026-11-21T23:11:02.242Z — exe-current-activity-autonomy-plan

reproduction: Inspect the dashboard running-process table while one or more MiMITA executables are active; change the dev-loop launch-mode picker or have a running session switch its mode; compare each row against that executable's current journal/runtime state, not startup arguments.
expected_behavior: For every MiMITA executable, show live process identity and observed current activity separately from its launch request: effective mode/map, connection/session state, match phase, roster/actor activity, latest meaningful action/event and age, evidence source and freshness. Unknown remains unknown. Expose the same data in a machine-readable query so an agent can select a process, collect its logs/identity, act through the authorized command surface, then verify owner events without manual UI inspection.
acceptance: Plan and implement per-executable live status from process identity plus each process runtime heartbeat/journal; update on transitions and mark stale when evidence ages. Show launch-intended and runtime-observed values in distinct fields. Support managed and external executables without inventing run linkage. Provide agent-readable JSON/status/command interfaces, attach current per-process journals and artifact identities, and make a Bloodtest agent able to discover an active Bloodtest process, inspect its current mode/map/actors/health/activity, reproduce a scoped scenario, and verify floor support/projectile/body-layer outcomes from the same run. Verify against multiple simultaneous executables, mode switch, idle/stale, process-scan fail/recovery, and managed/external paths.
paths: ['devscripts/dev-loop.py', 'devscripts/dev_loop/gui.py', 'devscripts/dev_loop/cli.py', 'src/network/server.cpp', 'src/network/server-gamemode.cpp', 'docs/specs/dev-loop/dashboard.md', 'docs/specs/dev-loop/multi-instance.md', 'docs/specs/dev-loop/dev-loop.md']
evidence: {'existing_issue': 'issue-c481ab36713a', 'new_context': 'ctx-a2766b80d9bd', 'scope': 'Extend existing launch-vs-runtime metadata issue with per-executable current activity and AI evidence workflow; avoid duplicate issue.'}
risk: A stale launch request can mislabel runtime mode and cause the agent to run wrong scenarios or claim proof against the wrong executable; process and journal identities must be revalidated, secrets excluded, and unowned processes remain observational only.
proof_expectation: Current runtime status and dashboard rows match OS process identities plus the exact per-process journals/events for multiple managed/external processes; mode changes, idle aging and stale scan recovery are reflected. Agent can autonomously discover/select/query/act/verify on a Bloodtest process with matching executable/DLL/build/run IDs and exact journal.
context_id: ctx-a2766b80d9bd

## Agent suggestion

### 2026-11-21T23:11:54.730Z — exe-current-activity-autonomy-plan

Plan for issue-c481ab36713a, extended per user context ctx-a2766b80d9bd:

Goal: make each live MiMITA executable self-describing enough that an agent can discover, select, inspect, act on, and verify the actual runtime without treating its launch request as current truth.

Phase 1 — identity and provenance contract. Inventory every managed and external process from a fresh OS scan. Key each row by PID + process creation time + executable path/hash. Keep requested launch mode/map/run config as a separate launch record. Link to a managed run/instance only by matching identity evidence; external processes remain external. Every displayed runtime field carries value, source, observed_at/age, and confidence/state (live, stale, unknown). Never infer a current mode from the dashboard launch picker.

Phase 2 — runtime-owned current state. Give each executable a queryable, low-cost runtime snapshot from its own live state: effective gamemode and map, match/lifecycle phase, active actor/roster counts and IDs where bounded, connection/session state, current fixed tick, last meaningful gameplay transition, active executable/build/DLL identity, and that process's exact journal path. Use state from current owners; the journal's last event alone is historical evidence and must not masquerade as an in-memory current snapshot. Refresh on transitions and a bounded heartbeat; age out stale snapshots visibly.

Phase 3 — dev-loop surfaces. Keep launch selection visibly labeled as requested launch config. Add separate observed runtime fields to each managed and external process row, detail pane, terminal status, and JSON status/inspection command. Refresh live process identity and runtime snapshot together; on failure retain last-known data as stale with explicit error/age and recover on next success. Do not adopt external processes, invent run IDs, or enable stop/restart for unowned processes.

Phase 4 — agent evidence workflow. Provide a documented machine-readable sequence: discover processes -> select exact PID/start-time/exe identity -> inspect current runtime snapshot and journal -> verify build/DLL/run/map/mode -> send a bounded action through the authorized dev-loop command/scenario path -> inspect same-process/server/client journals for owner acceptance and terminal outcome -> save a compact evidence bundle with timestamps, artifact IDs/hashes, process identity, inputs, journal paths/events/ticks, screenshots only when visual acceptance matters. The agent reports NOT_COVERED/unknown when required state or events are absent; it never substitutes launch intent, process responsiveness, build success, or an unrelated journal for runtime proof.

Phase 5 — focused proof and rollout. Verify two or more simultaneous MiMITA processes from different launch configs/checkouts; mode change after launch; idle process; stale/closed journal; process scan failure and recovery; managed and external processes; server/client disagreement and delayed networking; dashboard refresh/status JSON consistency. Then run one Bloodtest scenario on a current pinned artifact: identify actual mode/map/actors, verify living floor support, fire each physical projectile, and correlate authoritative projectile contact, damage/force, skin->meat->bone/part destruction and gore presentation across server/client journals. Keep visual acceptance separate. Roll out runtime snapshot first, then consumers, so the dashboard cannot claim state before the process owner publishes it.

Acceptance boundary: launch intent and live state must be visibly distinct; every live value has provenance/freshness; unknown stays unknown; agents can acquire the correct process and verify its actions against exact runtime evidence. This is a plan, not proof that any implementation or Bloodtest behavior is already working.

## Agent suggestion

### 2026-11-23T06:01:45.260Z — issue-review-sweeper

Proposal from the 2026-11-23 independent review of issue-cf9cd77eb056 (issue-review-sweeper). Observed evidence: at 2026-11-23T05:56:12Z `python devscripts/dev-loop.py --status` reported run=20261122_231306 as `run=RUNNING uptime=01:27:51` with `server pid=29536 alive=True` and `client pids=[17508]`, while `Get-Process mimita` returned no processes, port 64494 had no listener, `logs/2026-11-22/20261122_231306/run.json` was last updated 00:40:58Z, and the client journal ended with logger.stopped at 00:21:52.851Z. At 05:56:58.944Z `dev-loop.watcher.restarted` recorded `reason=daemon-not-alive old_daemon_pid=35436 new_daemon_pid=23824`. The agent-facing status therefore asserted a live managed pair for roughly five hours after the evidence ended. This is a direct safe-AI-capability gap: an agent can read RUNNING/alive=True and claim a runtime that does not exist.

Proposal (smallest change): make `--status` and the dashboard derive liveness from live evidence, not cached launch metadata. For each instance, require (a) PID alive with matching executable identity, (b) the bound endpoint present when the instance claims online, and (c) journal heartbeat within a stated freshness window. If any check fails, report `stopped` or `unknown` with `last_seen_utc` and the failed check, and never emit `RUNNING`/`alive=True` from the last launch record alone. Show launch-intended and runtime-observed fields separately, as the acceptance in this issue already requires.

Measurable target: 10/10 kill trials (managed server only, managed client only, both) produce `stopped`/`unknown` with a correct `last_seen_utc` within 5 s of the process exit, with zero false `RUNNING`/`alive=True`; and 10/10 healthy trials keep `online` without flapping. Smallest experiment: add the PID+executable+bind+journal-age check to the existing status formatter and run one kill/restart pair on this checkout; verify the status transitions and that a still-running external executable is not mislabeled as owned. No source was edited by this reviewer.
