# Fail slow game-DLL worker attempts sooner

- Changed the root dev-loop game-DLL worker timeout default from 3,600 seconds
  to 300 seconds. The existing supervisor kills the worker process tree and
  records a worker-timeout failure, so an unresponsive or very slow worker no
  longer occupies the build slot for up to 60 minutes by default.
- Kept `MIMITA_GAME_BUILD_TIMEOUT` as an explicit override for intentional
  longer experiments. The limit starts at worker launch; manifest and source
  preparation happen earlier and remain separately timed but unbounded by this
  setting.
- On the first real translation-unit compiler failure, the worker now stops
  launching queued units and terminates its already-running sibling compiler
  process trees. It records those interrupted units as cancelled and keeps the
  original compiler failure as the failure cause. Link-time failures cannot
  be detected before all required objects are available.
- The root watchdog restarted the daemon at 19:55:25.909Z (PID 30304) after
  the Python source changed. The shared dev-loop then naturally attempted
  `game-dll-20261124T200045986936Z-3f16b3`; I did not request or interrupt it.
- That attempt failed at 20:03:00.299Z, 134.336 seconds after worker start,
  with a real compiler error at `src/entities/player-render.cpp:634`
  (`layer` was not declared). The compile phase took 125.197 seconds; link and
  publication did not run. Worker progress records 8 sibling translation
  units as `cancelled`, alongside 1 failed, 34 compiled, and 595 skipped. This
  is direct evidence that first-error sibling cancellation works; the
  300-second timeout itself was not exercised.
- The automatic resource sidecar captured 99 samples over 134.387 seconds on
  12 logical CPUs: host CPU averaged 99.87% (peak 100%), RAM use averaged
  90.10%, and available RAM averaged 1.575 GB (minimum 160.9 MB). It aligned
  server PID 13680 and client PID 35812 to run `20261124_195553`, with fresh
  server-authoritative Bloodtest, 1 NPC/1 player; client mode was unverified.
  Client evidence near the end was 93.65 FPS, p95 32.04 ms, with 92/94 frames
  over 4.167 ms. This was not the target Juggernaut workload and is not a
  before/after latency or resource-causality result.
- A later shared attempt, `game-dll-20261124T200533130885Z-daf47f`, hit the
  same `player-render.cpp:634` compiler error and finished in 39.673 seconds
  total (35.392 seconds compile). Its attempt journal also records 8 cancelled
  sibling TUs. This independently confirms the compiler-error path exits well
  before 19 minutes under this workload.
- Another shared attempt, `game-dll-20261124T200649543312Z-74e74d`, completed in
  107.123 seconds. Its 14 needed TUs compiled and the 7.401-second link then
  failed on unresolved `MimitaNet` symbols. No compiler TUs were cancelled;
  the linker is the first stage that can establish these missing definitions.
  Its 91-sample sidecar measured host CPU mean 99.82% (peak 100%) and minimum
  available RAM 82.6 MB on 12 logical CPUs. It observed run `20261124_195553`,
  server PID 13680/client PID 35812, server-authoritative Bloodtest with
  1 NPC/1 player; client mode remained unverified. The adjacent client sample
  was 153.98 FPS with 148/154 frames over 4.167 ms and p95 11.665 ms. Neither
  run was the target Juggernaut workload.
- A further root-owned retry, `game-dll-20261124T201111864225Z-b63bfa`, failed
  in 7.380 seconds total, with LLD reporting the same missing `MimitaNet`
  symbols after a 2.351-second link. This shows that a repeated cached link
  failure can be surfaced in under 10 seconds. It does not prove a successful
  edit-to-live update; the batch remains failed and pending.
- Validation: Python syntax compilation and scoped diff checks passed. No
  separate test suite or manual timeout trial was run. The shared batch's
  compiler/linker failures were not edited or retried by this agent.

## 2026-11-24 follow-up: current root-daemon verification

- The root daemon PID 30304 started at 19:55:25Z, after `devscripts/dev_loop/game_build.py` changed at 19:51:09Z. The current daemon therefore loaded the 300-second default worker timeout.
- Latest observed attempt `game-dll-20261124T210028320585Z-d73143` succeeded as generation 621 in 125.149 seconds total (worker 111.262 seconds): two units compiled, 643 skipped; the changed `src/gamemode/arena-ball.cpp` preflight took 86.647 seconds, link 17.091 seconds, and publish 0.498 seconds. This is evidence about the current attempt duration, not a timeout test or a failed build.
- The previously recorded real failures provide the fail-fast evidence: compiler failures terminated in 39.673 and 134.336 seconds, and link failures in 7.380 and 107.123 seconds. The first compiler failure cancels active sibling compiler trees.
- Limit: the 300-second cap starts at worker launch. It does not impose one deadline across request setup, codegen checks, or scheduler waits, and no 300-second timeout path has been deliberately exercised. Diagnose any future long attempt by its exact attempt ID and last stage before changing the cap; valid translation units have taken roughly 149 seconds under severe memory pressure, so a much shorter hard per-worker cutoff could kill correct work.

## 2026-11-24 follow-up: stop waiting after a terminal worker failure

- The exact-attempt link failure in `game-dll-20261124T211225830289Z-dcfa7e`
  was persisted about 0.8 seconds before the parent watchdog fired at 300
  seconds. The previous correction preserved the error classification, but
  still made the user wait for the timeout.
- `devscripts/dev_loop/game_build.py` now watches the worker's
  `build/game/last-attempt.json` during the existing worker run. It reacts only
  to a complete `status=failed` record with a non-empty phase and error and the
  exact current attempt ID. It allows 0.5 seconds for normal worker shutdown;
  if the worker remains alive after that durable terminal failure, it stops
  the worker and returns a distinct terminal-failure code so the parent keeps
  the recorded phase and error.
- The 300-second watchdog remains for workers that have not recorded a
  terminal failure. This avoids killing valid slow compiles merely because the
  PC is under pressure. The change reduces delay *after* a failure is known; it
  cannot predict a compiler or linker error before that tool reports one.
- Added a synthetic hanging-worker regression case. The focused suite passes
  6/6; `py_compile` and scoped `git diff --check` pass. The synthetic failure
  returns in about 1.4 seconds, rather than waiting for its configured
  30-second timeout. No game build was requested to validate this Python-only
  supervisor change.
- Root daemon code has not been reloaded in this turn, so this new watcher is
  not yet confirmed active in the running dev-loop. The existing root daemon
  and game processes were left untouched; a safe controlled reload and a real
  observed terminal-failure attempt remain outstanding.

## 2026-11-24 follow-up: current DLL latency floor and activation mismatch

- The root daemon restarted as PID 19808 at21:57:02.852Z, after
  `game_build.py` was last written at21:50:41.326Z, so it imported the new
  supervisor code. I did not restart it again.
- Root attempt `game-dll-20261124T215807388728Z-84be9e` published generation625
  in182.661s from `src/main-systems.cpp` and
  `src/physics/physical-entity.cpp`: 3/645 compiled, 642 skipped. Each changed
  TU took60.48s; changed-source preflight took60.740s; object-cache scan1.778s;
  identity-provider compile6.657s; LLD link89.348s across646 objects/493.68MB;
  coherence0.742s; publish0.724s. This confirms cache reuse but misses the
  10-second target primarily on changed-TU compile and the monolithic link.
- The sidecar's144 samples measured99.98% mean/100% peak host CPU and free RAM
  minimum12MiB (last829MiB); pressure was severe, but causal impact on stage
  times is not isolated. The run identity is not a valid connected-pair proof:
  server PID5272 journaled Juggernaut/dust2cyberiav4 with67 NPC/0 players, while
  client PID16088 was associated with Vehicle Arena and child server PID10704.
  Server host DLL generation625 while NPC phase events still reported
  module_generation623. Thus successful publication/host activation does not
  prove NPC code activation.
- The exact phase split and the header-fanout/link-boundary experiment order
  were appended to the existing DLL latency issue. No source, compiler input,
  or runtime process was changed for this follow-up. The 10-second
  multi-file/live-pair acceptance goal remains open.

## 2026-11-24 follow-up: second build and stale local-module pointer proof

- Another root-owned attempt, `game-dll-20261124T220623155095Z-9c49ca`,
  published generation626 in319.167s from `src/effects/effect-part-blood.cpp`
  and `src/main-systems.cpp`: 3/645 units compiled,642 skipped. The two changed
  source TUs took175.381s and229.754s (414.198s summed worker compile time);
  changed-source preflight took229.948s wall; object scan0.189s; LLD39.544s
  over644 objects/493,460,824 bytes; coherence1.229s; publish0.916s.
- Its195-row resource sidecar recorded host CPU99.998% mean/100% peak, RAM
  available minimum651,264 bytes (0.62MiB), and runtime alignment unavailable.
  The current root run is Funcounterstrike on dust2cyberiav4 with server27372,
  client26492 and one NPC; it does not meet the target Juggernaut workload.
  No causal claim about resource pressure is made.
- Source review confirmed the activation blocker: resident `ModuleHost`
  resolves folded module APIs once and returns from polling for local modules;
  `HostRuntime` commits the new generation without rebinding that table. The
  resident `HostLoader` retains old HMODULEs in `retiredModules_`, explaining
  how stale callbacks can remain callable without crashing immediately. This
  means a successful DLL publication is not sufficient proof that NPC logic
  changed. The existing DLL-latency issue now records this exact call path and
  orders resident module rebind proof before the modular-link prototype.
- At the last status check, a game batch was pending, the live runtime was not
  a connected target pair, and available RAM was about140MiB. No new build,
  runtime start, or daemon restart was requested. Both the <=10-second
  multi-file build target and runtime activation proof remain open.
