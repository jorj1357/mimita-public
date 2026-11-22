# ctx-879a2e9d2b03 Idle/Server CPU Diagnostics and Plan

# Task

- Task ID: wake-5a515f494695
- Session ID: work-04e713241fc2 (agent cpp-latency-pressure-profile)
- Owner: cpp-latency-pressure-profile (diagnostics only; fix owner remains cpu-build-efficiency)
- Focus: high-priority user context ctx-879a2e9d2b03, "can u make diagnostics and run them and get as clear of a idea of what to do based on evidence", linked primary to issue-ed3ac775283d.
- Status: EVIDENCE_RECORDED_NO_SOURCE_CHANGE
- Date, time, timezone: 2026-11-22T02:54:00Z

# Pre-existing changes

- The shared checkout already contained extensive edits from other agents, including an active `src/npc/npc.cpp` lease and an in-progress isolated FCS proof runtime (`.dev/tmp/runtimes/fcs-proof`, PIDs 412 and 34976).
- This session edited no source or config, requested no build, and stopped no process. It preserved all running game processes.
- Files created: this changelog, run-scoped diagnostic CSV under gitignored `logs/2026-11-22/20261122_011031/diagnostics/`, and a temporary consolidated sampler outside the repository. Append-only issue discussion was written through `agent-coord.py`.

# Requested behavior

- Produce and run diagnostics that make the idle/normal-play CPU problem and the correct next actions clear from measured evidence.

# Findings

- The retained dev-loop server is the idle CPU regression: PID 26748, run 20261122_011031, map dust2cyberiav4, mode funcounterstrike, players=0, npcs=19. It consumed 86.8%, and earlier 101.8-106.6%, of one logical core (7.2-8.9% of the 12-logical-CPU machine) while no client was connected.
- The idle server is not a busy-wait: per-thread CPU is spread across many threads, and the cost is published by the server's own phases. `server.world-phase-window` reported npc_sim_ms 423-614, physical_contact_ms 422-1125, projectiles_ms 215-229 per one-second window; `npc.phase-window` reported physics ~169 ms, collision ~165 ms, decision ~120 ms, pose_containment ~104 ms, worst single NPC 163 ms.
- The playerless server keeps a full NPC-vs-NPC match running: 163-168 active/moving projectiles, `npc.target-changed` with `target_actor_type=npc`, and `server.damage.applied` with `attacker_type=npc`, `target_actor_type=npc`, `source=physical_contact`, including kills. The bots fight forever with zero players.
- The existing pause path in `src/network/server-npcs.cpp` (`simulateSharedNpcs`, around lines 1908-1932) pauses only for countdown, aftermath, or freezeDuringGo. There is no empty-room/player-count gate.
- The 60 Hz contract is already violated at idle: `performance.server-loop-window` showed actual_hz 3.7 and then 1.0-1.2, with loop_avg 1.35-5.16 s per tick.
- Concurrent instances add large load: at 02:38Z four `mimita.exe` processes consumed 316.4% + 151.2% + 115.6% + 86.8% of one core (~6.7 cores, ~56% of the machine) at 103.5% host busy. PIDs 34976/412 belong to another agent's isolated FCS proof pair; 26748 is the dev-loop retained server.
- The running server still uses pre-aggregation diagnostics (`module_generation=437`, `policy_generation=437`, per-tick `performance.server-phase-budget`) even though generation 441 is published, so the one-second aggregation change in `src/network/server.cpp` is not active in this runtime.
- Active play is a separate cost: the old client journal (PID 30496) shows 314-352% of one core at ~222-240 FPS, p50 frame 3.74-3.85 ms, p95 5.78-6.18 ms, p99 10.2 ms, with `EngineTick::Render` about 145 ms per 200 ms window dominant.

# Specification alignment

- Followed live-evidence-first: sampled the exact running dev-loop runtime and read its own journals; no synthetic self-test.
- No compiler, `build.py`, module worker, or second dev-loop was invoked. Build/source/runtime claims are kept separate.
- The <=1% whole-machine target is scoped to genuinely idle/background operation, not active rendering/simulation.

# Exact implementation changes

- None to source or config. Diagnostics and issue evidence only.

# Validation

- Build status: none requested.
- Runtime evidence: `devscripts/perf-watch.py` 15 s sample of PID 26748; a temporary psutil sampler for host/per-process/per-thread CPU and journal aggregation; journal queries on `logs/2026-11-22/20261122_011031/server-01/events.jsonl`.
- Issue record: message `msg-d5266769149d` appended to issue-ed3ac775283d; ctx-879a2e9d2b03 acknowledged.
- Process/proof state: no proof task created or verified, because no behavior changed.

# Measured evidence

- Idle server CPU: 86.8% (15 s, PID 26748) and 101.8-106.6% of one core in earlier single-server samples.
- Idle server phases per second: npc_sim 423-614 ms, physical_contact 422-1125 ms, projectiles 215-229 ms.
- Idle server loop: actual_hz 3.7 -> 1.0-1.2; loop_avg 1.35-5.16 s.
- Idle activity: players=0, npcs=19, 163-168 moving projectiles, NPC-vs-NPC target changes and kills.
- Concurrent load: four MiMITA processes, ~6.7 cores, 103.5% host busy.
- Evidence files: `logs/2026-11-22/20261122_011031/diagnostics/server-26748-idle-20261122T0236.csv`, `logs/2026-11-22/20261122_011031/server-01/events.jsonl`, `logs/2026-11-22/20261122_011031/client-01/events.jsonl`.

# Regression review

- Regression entry appended: no.
- Why: no behavior change was made.

# Human acceptance

- Visual review: not performed.
- Gameplay review: not performed; this was a read-only measurement of a running server.
- Still unverified: post-fix idle CPU and journal volume after the empty-room gate and a resident-build activation.

# Agent handoff context

- Fix owners: cpu-build-efficiency for issue-ed3ac775283d; launch/process lifecycle for retained/extra instances.
- Ranked next steps: (1) gate NPC AI/physics/collision/decision and NPC-driven projectile/contact work when connected players==0 for 2-5 s, keeping roster/snapshot/room/ICE responsive and resuming on join; (2) stop or suspend a retained server when its last player leaves and reap extra pairs; (3) activate the resident build so the `server.cpp` aggregation is live, then re-sample; (4) treat active-play client render/allocations as a separate issue; (5) dashboard refresh is secondary.
- Do not redo: this run's idle CPU sample, phase aggregation, and NPC-vs-NPC combat proof.

# How to reverse

- No source or build to reverse. Delete the gitignored diagnostic CSV only if it is not needed as evidence; preserve the append-only issue discussion entry.
