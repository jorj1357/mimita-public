# CPU hot-path and module-generation follow-up

- Session: `work-d947fb0eab83`
- Date: 2026-11-23
- Scope: read-only source and journal analysis; no source/runtime/build changes.

## Current live workload confirmation

The isolated run `20261123_195307` remains live on server PID `22184` and client PID `32832`; the root Bloodtest server PID `10164` is a separate process. The current server journal at 2026-11-23 21:25:31Z and 21:29:33Z reports Juggernaut Horde on `dust2cyberiav4`, 67 NPCs / 1 player, host DLL generation 571, and NPC phase `module_generation=570`. Server tick-window totals in those two examples were 23.44 ms and 30.72 ms; nearby client FPS records were about 54.8 and 39.6 FPS. These are journal diagnostics, not a host CPU sample or a 240 FPS/60 Hz acceptance run. Root dev-loop reports no active build; it does not own this isolated pair.

## Source findings

1. The `updateOneNpc` callsite probe at `src/npc/npc.cpp:1579` is an inclusive broad scope. Its `npcDecisionMs` timer stops at line 3080, before path/separation/physics/combat work.
2. The nearby-actor separation branch at `src/npc/npc.cpp:3498-3515` conditionally scans the full NPC vector for each targetless active NPC. A second full vector scan at lines 4112-4124 computes grenade friend/enemy density. At 67 NPCs these are plausible quadratic-scaling costs, but branch frequency and CPU share are unknown. No optimization is justified from this source shape alone.
3. NPC behavior module identity has a deeper reload path to resolve. `MIMITA_MODULE_GENERATION` points to the per-DLL identity provider (`devscripts/game_build_worker.py:830-847`); generation 571's provider contains 571. The folded NPC behavior API uses that macro (`src/npc/npc-behavior-module.cpp:947-951`). The resident `ModuleHost` caches local export pointers and `base->generation` in `src/hot-reload/module-host.cpp:785-884`, sets `mLocalModules=true`, then `startup` returns while `mStarted` and `poll` returns while `mLocalModules` (`:1104-1108`, `:1155-1158`). The game-module entry describes this as a resident ModuleHost resolving exports from the active DLL (`src/game/game-module.cpp:5`). This is a strong static explanation for why the local module may remain at 570 after host DLL 571 activation; pointer lifetime and refresh behavior still require confirmation. The separate 19:58 callsite profile was on generation 570 and must not be conflated with the later generation-571 baseline.

## Next measurement

After the active owners release a safe window, capture a generation-matched client/server pair and record CPU, memory, frame/tick telemetry, and bounded callsite windows. For the server, compare the broad `updateOneNpc` marker with non-overlapping sampled-cycle buckets around pre-physics decision/path work, the actor-separation scan, `physicsMainUpdate`, grenade-density scan/combat, and remaining tail. Include call counts, loop iterations, sample counts, same-process CPU milliseconds, and whole-host normalization. Verify module generation identity/pointer refresh separately; do not infer it from DLL publication alone. Repeat only after choosing one measured hot owner, and require gameplay correctness plus per-frame 240 FPS and 60 Hz server gates.

These findings were appended to CPU issue `issue-ed3ac775283d` and shared with the DLL-latency owner. The build remains held by that owner pending source-owner readiness; this session did not request a build, restart, or profile command.
