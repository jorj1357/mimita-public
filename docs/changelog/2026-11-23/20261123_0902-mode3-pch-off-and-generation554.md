# Mode 3 PCH-off comparison and generation 554 cleanup

## Goal and scope

Continue reducing live C++ edit-to-activation latency under the existing Mode 3 build policy while a Juggernaut Horde server/client pair remains active. Keep the root dev-loop as the only build authority, preserve MinGW and DLL hot reload, and measure the same 15 changed translation units across the PCH-on and PCH-off experiments. No gameplay behavior was changed.

## PCH comparison

The 15 benchmark source files received temporary comment markers, were built, then those markers were restored. All 15 sources had lower recorded per-TU compile time with PCH disabled, ranging from 30.8% to 92.7% lower. The three slowest remained `src/gui/ui-system.cpp` (19.09s), `src/gui/ui-system-buttons.cpp` (19.22s), and `src/gui/ui-text-input.cpp` (16.60s), so PCH removal helps but does not make these large translation units fast.

| Attempt | PCH | Compiled / skipped | Compile wall | Compile sum | Object scan | Link | Total attempt |
|---|---|---:|---:|---:|---:|---:|---:|
| `game-dll-20261123T081116995354Z-6190f6` (gen549) | on | 16 / 627 | 38.786s | 346.954s | 1.549s | 6.652s | 58.605s |
| `game-dll-20261123T084717741215Z-8e908f` (gen553) | off | 16 / 627 | 20.516s | 116.971s | 2.927s | 4.133s | 38.048s worker result; 39.816s request-to-ready |

This was not a perfectly load-matched A/B; treat the aggregate difference as directional. Every per-file comparison favored PCH-off in the observed runs, which is the evidence for retaining the 15 source-specific PCH exclusions. The existing `src/npc/npc-world-query.cpp` exclusion remains unchanged. The object key includes the PCH setting, preventing PCH-on objects from being reused as PCH-off objects.

## Restored-source cleanup build

After marker restoration, the root dev-loop built and published generation 554 from attempt `game-dll-20261123T085515045094Z-a48a01`:

- 16 compiled (the 15 restored source inputs plus generated module identity provider); 627 skipped.
- Request accepted 08:55:14.696Z; candidate ready 08:55:59.249Z: 44.553s.
- Client PID 10148 activated at 08:55:59.386Z: 44.690s after request.
- Server PID 8804 activated at 08:56:01.083Z: 46.387s after request. Candidate load began 08:55:59.127Z; server new-init took 1.955s before activation.
- Total attempt 43.693s; worker total 31.935s; changed-source preflight 22.304s; worker compile metric 23.382s.
- Object-cache scan 2.525s; LLD link 3.840s (3.516s linker plus 0.304s response-file preparation); publish 0.441s.
- Manifest snapshot 2.203s; manifest validation 0.013s; source snapshot 2.247s; resident-state codegen was a cache hit; packet-registry codegen 1.426s; coherence validation 0.514s.
- Object scan details: dependency-index prepass 0.692s; object freshness checks 1.456s; unique dependency stat prewarm 0.343s. The 627 skipped units still had 1.383s of dependency checks total (627 checks, 2.206ms average, 152.157ms max for `src/actor/player-ai.cpp`). The scan parsed 1,499 unique dependencies; dependency-index load/save took 0.096s/0.084s.
- The link consumed 642 object inputs totaling 483,418,586 bytes and 666 linker arguments; it is a full game-DLL link, not a one-object link.

The edit-to-live target remains unmet: this cleanup request took 44.690s to client activation and 46.387s to server activation. The dominant measured work is compiling/preflighting changed source files, followed by pre-worker setup, full-link time, object-cache scanning, and server reinitialization. Scan-only changes cannot close the remaining gap.

## Resource sample and runtime proof

The bounded sample covered 08:55:19.427–08:55:59.229Z (39.8s, 10 samples; 2.6–7.7s spacing due process enumeration). During the build, the 12-logical-CPU host averaged 92.4% CPU (81.6–100%), memory load averaged 68.0%, and available RAM averaged 5.01 GiB (minimum 4.56 GiB). From the same-row process counters, normalized once by 12 logical CPUs, server PID 8804 averaged 26.8% of whole-host capacity and client PID 10148 averaged 31.6%, combined 58.4%. Other observed load included MsMpEng PID 6696 (~4.7% host), pythonw PID 16864 (~4.0%), and python PID 22476 (~2.5%). The direct pair columns were reconstructed from the recorded same-row top-process values because the sampler's initial direct columns were zero; missing top-five values remain blank. This sampler is too coarse for thread attribution. The temporary CSV was initially written under `logs/2026-11-23/20261123_055730_01/dev-loop/mode3-cleanup-host.csv`, but that file and the run's `dev-loop/` subdirectory were absent during the final workspace check; why they disappeared is unknown. These summary statistics were retained in this changelog and the CPU issue, but the row-level sample is not currently available for replay.

The existing run remained `jugger_horde` on `dust2cyberiav4`; server journal reported 67 NPCs and one player. Both long-lived processes recorded generation 554 activation in `server-01/events.jsonl` and `client-01/events.jsonl`. This proves candidate activation and process continuity, not a gameplay behavior change. FPS/tick measurements were not aligned to this build interval. A bounded follow-up CPU profile was handed back to the CPU-reduction agent after cleanup.

## Result and next work

The PCH-off exceptions are retained in `devscripts/game_build_worker.py`; the 15 temporary source markers are restored. The main remaining blocker is changed-TU compile wall time (about 20.5–23.4s in the PCH-off runs), with request-to-worker setup around 8.8s, link around 3.5–3.8s, object scan around 2.5–2.9s, and server initialization around 2.0s. The whole DLL link still covers 642 objects. The next experiments should prioritize compile-path reduction and controlled linker/module-boundary options, with matched source sets and resource windows, while retaining MinGW DLL publication as the fallback.

Issue updates with the stage breakdown and resource evidence were appended to `issue-136d4553ff92` and `issue-ed3ac775283d`. The CPU agent was told that cleanup ended and invited to resume a bounded no-build profile. The active build path, compiler, runtime architecture, executable, and dev-loop authority were not switched; no EXE build or additional game instance was started.
