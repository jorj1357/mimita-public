# C++ latency: generation 500 compiler report investigation

## Root-loop evidence

Root dev-loop attempt `game-dll-20261122T144944189873Z-31ded0` published generation 500 from run `20261122_144609_01` in 39.164 seconds. The attempt changed `src/npc/npc-utility.cpp`, compiled 2 translation units (including the identity provider), skipped 641 of 642, scanned the object cache in 1.572s, linked 642 objects / 480,542,967 bytes with LLD in 8.494s, and published in 0.743s. The changed translation unit took 14.277s; changed-source preflight was 14.311s.

The attempt emitted the new `compiler_timing` field, proving that the instrumented worker ran in the root build. Its status was `empty`, however: GCC's timing header was detected but no phase rows were parsed. The prior event did not retain raw report text, so it cannot reveal the precise report layout. I added a bounded excerpt for future unparsed reports (at most 1,600 characters) and a focused regression test; this is diagnostic-only and does not affect compile flags beyond the existing optional report switch.

The manifest stage took 5.162s over 1,008 source and 1,217 header paths. Source enumeration took 1.976s because the persistent inventory had a watcher baseline gap: `invalidated_reason=directory_stamp_changed`, with `.`, `config`, and `config/accounts` changed, so the source inventory rescanned 1,745 directories. The header cache reported a hit but still spent 1.670s validating its 109 directory stamps during this daemon's first baseline. Digest validation reused cached stamps for 1,007/1,008 source paths and 1,216/1,217 headers; it hashed only the changed 23,145-byte source once. This confirms file hashing was not the dominant part of this manifest pass; startup inventory validation and cold enumeration were.

There were 11.432s from `build.started` to `worker_input_validation.started`. Named pre-worker stage durations sum to 7.276s (manifest 5.162s, validation .016s, source snapshot .792s, resident-state codegen cache hit reported 0s, packet-registry codegen 1.306s), leaving 4.156s not represented by those duration fields. The resident-state cache-hit start/end events are .315s apart despite the summary duration being zero. Event gaps and worker handoff account for additional wall time; the exact 1.676s after packet-registry completion and before worker input validation is a priority candidate for the next instrumentation pass.

## Validation

- `python -m py_compile devscripts/game_build_worker.py devscripts/quality/test_game_build_time_report.py` — passed.
- `python -m unittest devscripts.quality.test_game_build_time_report` — 7 tests passed.
- `git diff --check -- devscripts/game_build_worker.py` — passed with the existing LF-to-CRLF conversion warning.

## Limits and next step

The generation 500 build was produced for an FCS NPC behavior edit; this agent did not request it. The same server PID 4540 later emitted `game.hotness.summary` with `generation=500`, `server_path=dll`, 66 NPCs, zero players, zero server failures, and zero violations in `logs/2026-11-22/20261122_144609_01/server-01/events.jsonl` (for example, line 1356). This proves generation 500 was active in the server runtime, but it does not prove the changed NPC utility behavior achieved its intended result. The root dev-loop daemon and game processes remain live, with FCS/Horde and networking investigations active. Do not trigger an additional DLL swap until their runtime ownership clears. On the next naturally requested changed-source build, inspect the bounded excerpt and update the parser for the exact GCC output; then compare reported phase time with the 14.277s translation-unit wall time. Do not claim the 39.164s versus earlier 137.010s as a controlled optimization: the edited translation units and CPU load differ.
