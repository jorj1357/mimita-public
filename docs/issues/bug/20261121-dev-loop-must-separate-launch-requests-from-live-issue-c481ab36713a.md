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

## Scheduled independent review

### 2026-11-24T14:58:30.736Z — root-mode3-latency

A bounded independent review was queued as wake wake-8a2e14583dae after 576 completed work sessions (trigger=work_end). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (not_implemented_evidence_added)

### 2026-11-24T15:05:27.689Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Reviewer: issue-review-sweeper (bounded independent read-only review; no source edit, build, closure, or fix claim). All times UTC unless marked local.

Observed facts (2026-11-24):
- `issue readiness issue-c481ab36713a` -> ready=true, score 8/8, missing=[], state=new, tasks=4. Issue owner empty; reviews=[] before this append. All 4 tasks state=planned and latest_proof=null; `proof status` has no proof record for any task of this issue (the exempt=6/stale=27 rows belong to other tasks).
- Printed status at ~15:00Z (`python devscripts/dev-loop.py --status`): `run=RUNNING uptime=- visibility=visible`; `LAUNCH IDENTITY map=slaughterhouse1 mode=bloodtest gamemode=bloodtest weapon_set=- npcs=1`; `server pid=25840 alive=True uptime=02:46:12`; `client pids=[] uptime=-`. The only mode/map it prints is labelled LAUNCH IDENTITY; no runtime-observed mode/map/phase/players/tick field is printed.
- Independent OS check: PID 25840 alive (start 2026-11-24 07:13:34 local, C:\mimita-v9\.dev\current\mimita.exe); `dev-loop.py --daemon` PID 19372 alive. So `alive=True`/`RUNNING` here is truthful.
- Server journal C:\mimita-v9\logs\2026-11-24\20261124_121332\server-01\events.jsonl is fresh: `server.process.heartbeat` wall_time 2026-11-24T15:01:28.500Z with mode=bloodtest, map=slaughterhouse1, npcs=1, players=0, projectiles=0, room_code=PM7NRQX, tick=602743; latest `network.mode.start` has mode=bloodtest, map=slaughterhouse1, phase=4, stage=waiting, waiting_for=required_clients, active_players=0, npc_count=1.
- Agent-readable JSON (`dev-loop.py --status --json`, 518 KB): top level has `effective_map=slaughterhouse1`/`effective_mode=bloodtest` (launch identity), `server_alive=True`, `run_alive=True`, `run_uptime_seconds=` (empty), `server_uptime_seconds=10164`. Its `instances[server-01]` reports state=online, pid=25840, bind=127.0.0.1:53450, map="" (empty), mode="current", event_age_ms=1.7, last_event_utc=2026-11-24T15:02:58.668Z, roster_count=0, build_match=false. There is no observed_mode/observed_map/observed_phase/observed_players/observed_tick/heartbeat_age field.
- Endpoint check: PID 25840 owns a live UDP endpoint 127.0.0.1:53450 (and ::53451); server args use `--bind 127.0.0.1:0`. So `state=online`+`bind=127.0.0.1:53450` IS backed by a real UDP endpoint; I found no false-online case here.
- `.dev/server-owner.json` records the actually launched mode: `run_metadata.launch_mode=59`, `launch_mode_name="Bloodtest - Slaughterhouse 1 with NPC"`, map=slaughterhouse1, resolved local_port=53450. By contrast the current status top level `launch_mode_name="current"` and `instances[server-01].mode="current"`.
- Read-only code cause: devscripts/dev-loop.py:1973 sets the instance `map` from `self.current_map`; `self.current_map` defaults to "" (line 1500) and is populated only during a fresh launch (line 5021), so an adopted server (server_adopted=True) renders map="". devscripts/dev-loop.py:1974 sets the instance `mode` from `self.launch_mode.get("name", ...)` - a launch selection, not runtime state. `_instance_records` reads only the journal's last/event age (lines 1947-1948, 2017-2020) and never reads `server.process.heartbeat`/`network.mode.start` mode/map/phase/players/tick. `_print_read_only_status` (line 8068) prints only LAUNCH IDENTITY plus pid/alive and never prints `instances` or any observed field.

Inference / uncertainty:
- The acceptance clause "Show launch-intended and runtime-observed values in distinct fields" is unmet on both the printed and JSON status surfaces used here: observed mode/map/phase/players/tick exist in the fresh heartbeat but are not surfaced; launch identity is the only mode/map shown. This is a consumption gap, not a data-availability gap in this sample.
- Instance `mode="current"` and top-level `launch_mode_name="current"` disagree with the run's actual mode 59 ("Bloodtest - Slaughterhouse 1 with NPC"). Whether "current" is a real configured launch-mode name or a placeholder is unknown to me; either way the status surfaces disagree with .dev/server-owner.json, so an agent reading status cannot tell it was launched in mode 59.
- `builds_match=false` is consistent with `client_ready=false` (no client), per dev-loop.py:5378, not with a build-ID mismatch: `running_build_id == latest_successful_build_id` (both 2026-11-24T07:48:35.684015Z:52c3ec0a...). The field name overstates what it means (tech-debt candidate, not this issue's core).
- `run_uptime_seconds` empty while `run_alive=true` and `server_uptime_seconds=10164` is a minor freshness inconsistency.
- Because runtime mode/map currently equal the launch values, this run cannot demonstrate a launch-vs-runtime divergence; it demonstrates the observed data is available and fresh yet unconsumed.

Contrary evidence:
- The 2026-11-23 review's ~5-hour stale-RUNNING defect is materially improved: `server_health` (dev-loop.py:4413) revalidates an adopted PID by executable path and `--server` command line; `server_adopted=True`; a `dev-loop.process.adopted` event at 2026-11-24T14:32:04Z matched persisted marker + live identity. Do not re-report that defect as unaddressed without a fresh kill trial.
- Online/bind here is genuine (live UDP 53450 owned by PID 25840), so no false-positive liveness is shown in this sample.
- The issue already has a 2026-11-21 phase plan and a prior review; this append is current runtime evidence, not a new plan.

Next discriminating check (read-only for a reviewer; owner-level for a fix):
- Populate per-instance observed fields from that instance's bound journal (`server.process.heartbeat` / `network.mode.start`: observed_mode, observed_map, observed_phase, observed_players, observed_tick, last_heartbeat_utc, heartbeat_age_ms) and restore/derive `current_map` on process adoption. Then re-run `dev-loop.py --status --json` and require `instances[server-01].observed_map == heartbeat.map` and `.observed_mode == heartbeat.mode`, while LAUNCH IDENTITY stays separate. Falsifiable: after an in-game mode switch, observed_mode must track the new heartbeat mode while LAUNCH IDENTITY stays the original; if status still shows the launch value or empty, separation remains unimplemented.
- This exact run is a reproduction for the empty map: adopted server shows `instances[].map=""` although heartbeat and run_metadata agree map=slaughterhouse1. Falsifiable: if an owner shows `current_map` is repopulated on adoption, that finding is disproved.

No source was edited, built, or closed; this is a read-only review.
