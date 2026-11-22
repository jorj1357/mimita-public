# CPU/core-claiming history corroboration (ctx-20f2a628316f)

## Task

User asked to investigate the memory that "some days ago" MiMITA changed CPU use and started claiming cores. Woken agent `fun-cs-npc-objective`; context was already linked primary to `issue-ed3ac775283d` (owner `cpu-build-efficiency`), so routing needed no repair.

## Read-only findings

- No CPU-affinity call exists in current `src`, `devscripts`, or `config`. Live `mimita.exe` PIDs report Windows `Normal` priority with `ProcessorAffinity=0xfff`, i.e. all 12 logical CPUs allowed and none reserved.
- Confirmed the build-concurrency history another audited agent recorded: `d4898e0` (2026-11-19) made `--jobs 0` resolve to `os.cpu_count()`; `d89884f` (2026-11-21) changed the root launcher to `--jobs 10`. This only affects compile-time demand, not persistent game CPU.
- Stronger match to "claim like 1 core": the retained playerless dedicated server. PID 26748 (run `20261122_011031`, 0 connected clients) used 4.70 CPU-s over 5.0 s = 94.1% of one core. Its journal showed `performance.server-loop-window actual_hz=1.30` and `npc.phase-window total_ms=394-748`. The cost is empty-room NPC/gamemode/projectile simulation, not pinning or a frame-pacer spin.

## Changes

- No source, config, build, or runtime process was changed.
- Appended one corroborating suggestion to `issue-ed3ac775283d` and acknowledged `ctx-20f2a628316f`.

## Evidence and limits

- `python devscripts/dev-loop.py --status`: server PID 26748 alive, client PIDs empty.
- Process sample: 5-second CPU delta on PID 26748 plus `ProcessorAffinity` for live `mimita.exe`.
- Journal: `logs/2026-11-22/20261122_011031/server-01/events.jsonl` tail.
- This does not prove the user's exact remembered commit; it rules out affinity and identifies the actual one-core consumer. The owner's existing empty-room suspend task remains the fix.
