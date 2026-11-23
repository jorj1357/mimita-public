# Edit-to-live C++ latency pressure follow-up

Date: 2026-11-22

## Target

Reduce `.cpp` save-to-visible-runtime activation to 10 seconds or less under measured Juggernaut/AI resource pressure, preserving correct cache invalidation, stable module identity, the existing MinGW whole-game DLL fallback, and the root dev-loop build authority. This follow-up records evidence and the next optimization gate; it does not claim the target is met.

## Controlled one-file canary

Build attempt `game-dll-20261122T224135400285Z-21907f` published generation 540 successfully. Source mtime was 22:41:35.347Z, worker/build start was 22:41:35.401Z, and publication completed at 22:42:14.140Z: approximately 38.79 seconds from save to publish.

Only `src/npc/npc-world-query.cpp` was listed as changed. The build skipped 641 cached objects and compiled two objects. Timings from `.dev/build-timings/game-dll-20261122T224135400285Z-21907f.json`:

- Before first worker output: 6.168 s. Manifest snapshot: 1.176 s; manifest validation: 0.018 s; source snapshot: 0.833 s; packet registry codegen: 0.838 s. Resident-state codegen was a cache hit.
- Changed source preflight/compile: 20.066 s for the changed TU.
- Module identity provider compile: 1.540 s.
- Object cache scan: 1.785 s across 641 candidate objects; dependency prepass 0.541 s, freshness checks 0.852 s, unique dependency stat prewarm 0.352 s.
- Full DLL link: 8.170 s (642 objects, approximately 482.6 MB linked).
- Coherence validation: 0.332 s; publish: 0.376 s.

The attempt missed the 10-second target by about 28.8 seconds. It succeeded as a build, but generation 540 activation and edited-function behavior were not proven in the live runtime. Earlier independently authored generations 538 and 539 activated in the same Juggernaut server 0.591 s and 0.318 s after publication, respectively; those are observed reload-stage timings, not controlled same-source builds.

## Pressure sample and safety

A six-second process sample observed 100% host CPU across 12 logical processors, four MiMITA processes using approximately 77%, 133%, 266%, and 151% of one core, RobloxPlayerBeta using 183%, and 2.66 GiB available RAM. A separate active Juggernaut test run belonged to another agent; I did not start, stop, or change its processes. The dedicated earlier pressure run was stopped through the root dev-loop after free memory fell to 0.27 GiB. That stopped run had proved activation only through generation 539, not 540. The process sample demonstrates concurrent pressure, but it does not isolate each process's CPU contribution to the compiler.

## Diagnosis and next experiment

The cache scan is not the current one-file bottleneck: 1.785 seconds is material but far below the 20.066-second changed-TU compile and 8.170-second full DLL link. Before-worker work is also substantial at 6.168 seconds. Even eliminating compile alone would leave a full link plus fixed work above 10 seconds. The first useful implementation experiment is a per-translation-unit PCH opt-out with a distinct compiler-flags/cache identity and unchanged global MinGW fallback, measured against a matched PCH-on edit. The 10-second end-to-end target likely also needs a hot behavior module path that links only a small module rather than relinking the 642-object whole-game DLL. Do not disable PCH globally or weaken dependency correctness to make scans appear faster.

The exact Gen540 timing, CPU caveat, and runtime-proof gap were appended to `issue-136d4553ff92`; pressure context was added to `issue-ed3ac775283d` and `issue-be39a4f5ee17`.

## Scope and verification

No compiler, build system, game architecture, game process, or dev-loop configuration was changed in this follow-up. No new build was requested because the current high-pressure Juggernaut run is owned by another agent, memory had recently dropped below 0.3 GiB, and the active build-worker source has concurrent edits. The next experiment should wait for safe free-memory reserve and coordinate runtime ownership, then use the root dev-loop with one isolated source change, matched stage timings, and same-PID journal proof of activation and intended behavior.
