# Issue-cf9cd77eb056 Independent Review (2026-11-23)

# Task

- Task ID: bounded independent review of issue-cf9cd77eb056 (wake-82854ea8a215)
- Owner: issue-review-sweeper
- Focus: end-to-end MiMITA development and networking consistency; newest pinned run and current dev-loop status truthfulness
- Summary: Reviewed issue readiness, proof status, linked artifacts, and the latest relevant journals. Appended one evidence review (verdict `evidence-gap: latest pinned run fails client-stall acceptance; proof unlinked`) to issue-cf9cd77eb056 and one measured liveness-truthfulness proposal to issue-c481ab36713a. No source, build, runtime stop, close, or fix was performed.
- Status: REVIEW_RECORDED_OPEN
- Date, time, timezone: 2026-11-23T06:01:40Z

# Pre-existing changes

- The shared checkout already contained modifications from other agents (`devscripts/blender-ai`, issue documents from the coordination tool). This session edited no source and reverted nothing.
- Files created by this session: this changelog. The issue review and suggestion were appended through `agent-coord.py`, which also updated the two issue documents.
- The dev-loop watcher automatically restarted a wedged daemon and launched a new root run during the review; that was not reviewer action.

# Requested behavior

Complete one bounded independent review of issue-cf9cd77eb056 without editing source, building, closing, or claiming a fix; record observed facts, inference, uncertainty, contrary evidence, and the next discriminating check, plus a measured process proposal when supported.

# Findings

- Readiness is `ready=true`, `score=8`, `missing=[]`, state `in_review`, 12 tasks, two prior reviews. `proof list --issue issue-cf9cd77eb056` returns `[]`: no proof record is linked. `related_build` still names generation 446 while the newest isolated pinned run is generation 545 and the dev-loop published generation 546.
- Latest isolated pinned run `20261122_231658` (EXE `e24aaf16...`, game generation 545, FCS/dust2cyberiav4, 19 NPCs): configuration_load 322.4111 ms, world_load 3211.1439 ms, network_bind 0.9173 ms, room_register 590.8082 ms (room P3MTGWS at 23:17:03.126Z). Server process start 23:16:58.717Z gives ~4.41 s to code; registration is under 1 s but total start-to-join is not.
- Same run, client: 1641 applied snapshots; max `snapshot_interarrival_ms` 1493; zero intervals >=2500 ms; zero `network.client.snapshot.gap`; two Connecting->Connected transitions (23:17:07.872Z and 23:18:38.379Z after intentional leave/rejoin). The reconnect/stale-peer path was not exercised.
- Same run, client acceptance: 7564 `performance.frame-slice` samples include 83 with `frame_max_ms` >250 ms and 1412 >100 ms; max 2854.109619 ms at 23:17:08.862Z (npcs=0, players=1). The acceptance phrase "no >250ms client stall" is therefore not met as written.
- Same run, server: 635 `performance.server-tick-stall` events >100 ms, 72 >250 ms, 16 >500 ms, max 1036.77 ms at 23:42:31.386Z; round-start tick 10262 was 710.704 ms total with gamemode_ms 621.847 and snapshot_ms 0.265. Stalls are not snapshot-stage, consistent with no client snapshot gap.
- Agent-facing status was false at 05:56:12Z: `dev-loop.py --status` reported `run=RUNNING uptime=01:27:51` and `server pid=29536 alive=True` for run 20261122_231306 while no `mimita` process existed, port 64494 had no listener, and the journals ended at 00:40:58Z (server) and 00:21:52Z (client). At 05:56:58.944Z `dev-loop.watcher.restarted` recorded `reason=daemon-not-alive old_daemon_pid=35436`.
- The watcher's new run 20261123_055730_01 loads generation 546 (`game.host.probe.started` seq6) and had a client online with `snapshot_age_ms` 525 by 05:58:52.924Z. No isolated pinned test `run.json` reports game generation 546.
- The current run is mode 44 juggernaut horde with `server.policy.startup` `npc_count=1`, `map_config_load` `startup_npcs=1`, one observed `npc.spawn.resolved` (id 1000), and gamemode capacities fighters 64 / juggernauts 4. Launch args include `--npcs 1` and duplicate `--mode` (`ssbm` then `jugger_horde`). The one-NPC symptom is reproduced by launch configuration, not proven as a gamemode spawn defect.

# Specification alignment

- Followed AGENTS.md gates 0b/0c/4: read coordination state, kept build evidence separate from behavior proof, made no source/build change, and did not create a duplicate issue.
- Preserved all prior issue text; the review and suggestion are append-only.

# Exact implementation changes

- None. Append-only issue review and suggestion only.

# Validation

- Build status: no build requested; no compiler, worker, build script, or dev-loop build invocation.
- Evidence read: issue brief/show/readiness, `proof list`, coordinator SQLite reviews, `logs/2026-11-22/20261122_231658/**` (run.json, server/client journals, dev-loop events), `logs/2026-11-22/20261122_231306/**`, `logs/2026-11-23/20261123_055730_01/**`, live process and listener checks, and `dev-loop.py --status`.
- Review artifact: issue-cf9cd77eb056 review appended, verdict `evidence-gap: latest pinned run fails client-stall acceptance; proof unlinked`.
- Process proposal: appended to issue-c481ab36713a (live process metadata liveness).

# Measured evidence

- Run 231658: config 322.4111 ms, world_load 3211.1439 ms, room_register 590.8082 ms; code at 23:17:03.126Z, process at 23:16:58.717Z.
- Client 231658: 1641 applied snapshots, max interarrival 1493 ms, zero >=2500 ms; 83 frame-slice samples >250 ms, max 2854.109619 ms.
- Server 231658: 635 stalls >100 ms; 72 >250 ms; 16 >500 ms; max 1036.77 ms; round tick 10262 = 710.704 ms (gamemode 621.847, snapshot 0.265).
- Status truthfulness: false `RUNNING`/`alive=True` at 05:56:12Z; watcher `daemon-not-alive` restart at 05:56:58.944Z; new run online at 05:58:52.924Z.
- Current run: generation 546 loaded, juggernaut horde, npc_count=1, startup_npcs=1, launch `--npcs 1`, duplicate `--mode`.

# Regression review

- Regression entry appended: no. No behavior or code change was made.

# Human acceptance

- Visual review: not performed.
- Gameplay review: not performed.
- Still unverified: whether the >250 ms client frames are render/asset/network, whether the horde launch profile is the intended roster, and whether the status contract will be enforced.

# Agent handoff context

- Owner: root-connection-reliability on issue-cf9cd77eb056; issue-c481ab36713a has no owner and now carries the liveness proposal.
- Next exact step: fresh isolated pinned gen546 run with client frame-scope capture for the >250 ms frames; then a horde run without the forced `--npcs 1`/duplicate `--mode`; then a status kill/restart check under owner approval.
- Do not redo: the 231658 counts, the 05:56 status/process comparison, or the current-run launch-arg inspection.
- Do not close as fixed without a linked proof or reasoned exemption; the issue's proof list is empty and its `related_build` is stale.

# How to reverse

- No source or build to reverse. Remove this changelog only through normal review if created in error; preserve the append-only issue review and suggestion.
