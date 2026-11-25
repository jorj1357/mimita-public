# Generation 635 compile cost attribution

## Observed build

Attempt `game-dll-20261125T023802184416Z-39264c` started at 2026-11-25 02:38:02 UTC and completed in 254.029 seconds in its worker summary (255.459 seconds in the resource summary). It changed three `.cpp` files plus `src/combat/weapon-fire.h`; 17 translation units compiled. The header caused 13 exact dependent invalidations.

The measured phase wall times were 116.777 seconds in changed-source preflight, 95.041 seconds in the remaining compile phase, 26.332 seconds in LLD link, 2.576 seconds in object-cache scan, and 0.577 seconds in publish. The slowest translation units were:

| Translation unit | Compiler wall | GCC preprocessing | GCC parsing | GCC overload resolution |
|---|---:|---:|---:|---:|
| `src/ragdoll/ragdoll-mode.cpp` | 116.526 s | 21.14 s | 37.36 s | 7.27 s |
| `src/combat/weapon-system.cpp` | 79.780 s | 21.25 s | 36.64 s | 3.65 s |
| `src/combat/weapon-fire-raycast.cpp` | 77.684 s | 22.34 s | 38.31 s | 4.37 s |

Only 3 of the 17 compiler records contained parsed GCC phase timing. Other dependent units took up to 65.387 seconds. This shows that header fanout was one contributor, while directly edited translation units and the monolithic link also consumed substantial time.

The build sidecar recorded 12 logical CPUs, mean host CPU 99.931%, and minimum available RAM 52,178,944 bytes. It aligned to run `20261125_022449`: server PID 6244 reported Bloodtest, 1 NPC, 1 player; client PID 38720 had stale/unreported mode. There was no matched pre-build baseline. Resource pressure is correlated context, not proven cause of the compile timings.

## Historical preflight correction

The attempt ran at 02:38 UTC. The current `source_only_preflight_eligible` guard was committed later, at 00:15 local time (05:15 UTC). It allows early preflight only when the change report proves the batch contains existing `.cpp` content changes and no header/source-set/ownership changes. The current root coordinator launches `devscripts/game_build_worker.py` as a subprocess, so a later attempt should use that current worker source.

The 116.777-second preflight is therefore evidence of the older mixed-batch behavior, not evidence that the current source still serializes a mixed header/source build. A future root build must verify that a mixed header batch skips this separate preflight and compiles the complete stale set in one bounded wave.

## Outcome

This was an evidence review only. No source or build configuration changed, no build was requested, and no runtime activation or behavior proof was produced. The measurements and corrected interpretation were appended to `issue-136d4553ff92`.
