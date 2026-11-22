# ctx-4f1aa0089f64 Active-Gameplay CPU Evidence

# Task

- Task ID: wake-f6c8b7160447
- Session ID: work-cd6c34e6bf0a
- Owner: codex-network-mode-launch
- Focus: high-priority user context ctx-4f1aa0089f64 ("Also during active gameplay") linked primary to issue-ed3ac775283d, plus its relation to the networking slowdown/reconnect issue issue-cf9cd77eb056.
- Status: EVIDENCE_RECORDED_NO_CODE_CHANGE
- Date, time, timezone: 2026-11-22T01:45:00Z

# Pre-existing changes

- The shared checkout already contained extensive modifications from other agents. This session edited no source and reverted nothing.
- Files created by this session: this changelog, run-scoped host resource CSVs under gitignored `logs/2026-11-22/20261122_011031/`, and a temporary SQLite query file outside the repository. Append-only issue discussion entries were written through `agent-coord.py`.

# Requested behavior

- Review the new active-gameplay scope on ctx-4f1aa0089f64, acknowledge it, and add bounded measured evidence to the owning issue without editing code.

# Findings

- Context routing is correct: ctx-4f1aa0089f64 is linked primary to issue-ed3ac775283d, owned by cpu-build-efficiency, readiness ready with no missing fields. The scope statement adds active gameplay on top of the issue's idle/background target.
- Running runtime for this sample: run 20261122_011031, launch_id 2026-11-22T01:10:31.858Z, build 2026-11-21T02:26:50.306117Z:8b5838cbbf4c1bcda47c3f35e33583fde83bdacc89bdc52961e13980c976b871, map dust2cyberiav4, mode funcounterstrike. Dev-loop reported source_changed with a game-dll build pending, so the numbers are the pre-build artifact.
- Host has 12 logical processors and 83-84% memory load during the sample.
- Client PID 30496: mean 351.2% of one core (min 323.6, max 392.6) = ~29.3% of the machine; working set ~752 MB; journal 1s window avg_fps 213.7, p50 4.49 ms, p95 5.78 ms, p99 6.13 ms, frames_over_budget 146/214 against the 4.1667 ms 240 FPS budget.
- Client per-60-frame attribution: EngineTick::Render 223.7 ms, Rendering 160.8 ms, Swap 44.8 ms, Rendering::Actors::NetworkEntities 78.9 ms with about 42,360 allocations, PlayerOutline 28.7 ms, Shadows 22.8 ms.
- Server PID 26748: mean 102.1% of one core (min 56.3, max 140.1) = ~8.5% of the machine; working set ~79 MB; 423 tick windows and 939 stalls; recent windows average 43-100 ms with p95 94-232 ms; stall ticks spend 183-230 ms in the npc scope at players=1 npcs=19 against the 16.67 ms 60 Hz budget.
- Net active-gameplay cost ~453% of one core = ~38% of the 12-core machine. This defines the active-gameplay floor; it does not contradict the <=1% idle/background target.
- Low-confidence observation for networking: client perf windows report npcs_active=0 while the server has 19 NPCs; this needs tracing before being treated as missing remote entities.
- Another agent recorded an independent matching baseline (server ~54% of one core, client ~314% of one core) on the same run before the resident EXE rebuild.

# Specification alignment

- Followed the live-evidence-first rule: sampled the exact running dev-loop runtime and read its own journals rather than creating a synthetic self-test.
- No build, compiler, module worker, or second dev-loop was invoked. Source/build/runtime claims are reported separately, and the pending source change is called out.

# Exact implementation changes

- None to source or config. Evidence and discussion entries only.

# Validation

- Build status: none requested; a game-dll build was already in progress.
- Runtime evidence: `devscripts/perf-watch.py` 20-second samples at 1 s interval, one per process, plus `devscripts/perf-evidence.py` on client-01 and server-01 journals.
- Issue records: message msg-2542fd86c372 appended to issue-ed3ac775283d; message msg-a0f3ad83140b appended to issue-cf9cd77eb056; ctx-4f1aa0089f64 acknowledged.
- Process/proof state: no proof task was created or verified, because this session did not change behavior.

# Measured evidence

- Client CPU: 21 samples, mean 351.2%, min 323.6%, max 392.6% of one core.
- Server CPU: 21 samples, mean 102.1%, min 56.3%, max 140.1% of one core.
- Server stalls: 939, with the worst sampled tick 231.9 ms total and 230.2 ms in the npc scope.
- Evidence files: `logs/2026-11-22/20261122_011031/host-resource-samples-client-30496.csv`, `logs/2026-11-22/20261122_011031/host-resource-samples-server-26748.csv`, `logs/2026-11-22/20261122_011031/client-01/events.jsonl`, `logs/2026-11-22/20261122_011031/server-01/events.jsonl`.

# Regression review

- Regression entry appended: no.
- Why: no behavior change was made; this session only recorded measurements and issue discussion.

# Human acceptance

- Visual review: not performed by this session.
- Gameplay review: not performed; the sample was taken from a running live pair, not played or judged.
- Still unverified: the source-level hot owner after the pending build, and whether the server tick stalls cause the observed reconnect sequence.

# Agent handoff context

- Owners: cpu-build-efficiency for issue-ed3ac775283d; root-connection-reliability for issue-cf9cd77eb056.
- Next exact step: after the pending game-dll build lands, re-run the same 20 s sampling and journal read on the new build, then profile server per-NPC update at 19 NPCs and client Rendering::Actors::NetworkEntities allocation/bind work.
- Do not redo: this run's process sample, journal summary, and context acknowledgment.

# How to reverse

- No source or build to reverse. Delete the gitignored run-scoped CSVs only if they are not needed as evidence; preserve the append-only issue discussion entries.
