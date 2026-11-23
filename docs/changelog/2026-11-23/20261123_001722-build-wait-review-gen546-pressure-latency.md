# Build-wait review: gen546 pressure latency

**Date:** 2026-11-23 UTC
**Agent:** build-wait-reviewer
**Issue:** `issue-136d4553ff92` (cross-linked `issue-ed3ac775283d`)
**Run:** `20261122_231306`
**Build reviewed:** `game-dll-20261122T235222465818Z-9bfbf7`, generation 546

## Result

The build succeeded (267.2 s) and generation 546 is live in server PID 29536
(Juggernaut Horde, 67 NPCs, 0 failures, 0 hotness violations). This session did
not edit code, start a build, or claim the 10 s target.

## Stage evidence

- Changed TU `src/network/server-gamemode.cpp` (543 KB, 10,667 lines) compiled in
  151.1 s, 56% of the total. GCC `-ftime-report` for it: opt-and-generate 86.4 s
  (63%), parsing 39.4 s, preprocessing 19.2 s, template instantiation 16.7 s.
- Link: 59.7 s for ~483 MB of objects, vs 8.0 s in gen545 with the same inputs.
- Object scan 1.6 s; publish 0.49 s. These are already fast.
- Generated 0.3 KB `module-identity-provider.cpp` (already compiled PCH-off with
  `-O0 -g0`) took 38.6 s vs 0.679 s in gen545 for the identical command (57x).

## Resource evidence (direct, 12.4 s sample at 2026-11-23T00:09:37Z)

- 12 logical CPUs, host total CPU 100%, free RAM 0.37-0.79 GB of 15.65 GB.
- Server 2.92 cores (24.3%); client 3.61 cores (30.1%); live game pair ~54% of
  the machine. Dashboard 0.21 core, daemon 0.22 core, 7 opencode ~0.71 core.

Inference: identical compile/link work slowed 7.5x-57x between gen545 and gen546
with the same inputs, so tonight's dominant multiplier is host CPU/memory
pressure (pagefile thrash of the 115 MB PCH and GCC working set), not the build
algorithm.

## Follow-ups recorded (no code changed)

1. Relieve host memory/CPU pressure during builds; this is the prerequisite for
   any 10 s claim.
2. Benchmark PCH-off more broadly; `npc-world-query.cpp` already proved
   20.0 s -> 1.06 s for a thin TU.
3. Use `-O0` for debug changed TUs; opt-and-generate is 86.4 s of 151 s.
4. Split/shrink the 543 KB `server-gamemode.cpp` and/or use a coarse module path
   to avoid the ~483 MB monolithic relink; even a good-condition link is ~8 s.
5. Add an explicit `game.generation.activated` journal event; activation is only
   upper-bounded today.
6. Queueing/retry friction (3 failed `packet_registry_codegen.py` pre-worker
   attempts, over-budget waits) probably explains the reported ~30 min
   edit-to-test better than the 4.5 min build itself.

## References

- `docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md`
- `.dev/build-timings/game-dll-20261122T235222465818Z-9bfbf7.json`
- `.dev/build-timings/game-dll-20261122T231534497789Z-06de62.json`
