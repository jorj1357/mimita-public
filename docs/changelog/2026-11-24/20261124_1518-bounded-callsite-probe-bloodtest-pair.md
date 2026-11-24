# Bounded callsite CPU probe on Bloodtest pair

## Work

- Ran one bounded 15-second server callsite CPU probe on the existing connected pair after the Mode 3 owner authorized the sample.
- Recorded the profile in the CPU issue with exact workload, generation, sampling, tick, and admin-state limitations.
- Made no source edits, builds, process priority changes, or runtime restarts.

## Result

- Fourteen complete 1/64 sampled windows, 2026-11-24T15:18:47.212Z–15:19:01.069Z, on server PID25840: Bloodtest/slaughterhouse1, 2 NPCs/1 player, host DLL g594 and NPC module g593.
- `updateOneNpc`: 581.74 estimated CPU-ms, 2.46% of server CPU, ~0.326% whole-host; 27 samples.
- `doCollisions`: 84.58 estimated CPU-ms, 0.36% of server CPU, ~0.047% whole-host; 28 samples.
- Both scopes combined account for only an estimated2.82% of server CPU in this sparse, non-target sample. This does not identify the remaining CPU or prove a fix.
- Server recorded 793 ticks in13 tick windows (60–62 ticks/s); tick-window average2.083ms, worst p95 11.035ms, maximum tick20.183ms. No client frame samples occurred in the probe interval.
- Admin command setting is disabled, but the connection grant remains true. Full revocation was not performed; the server source says host-only revoke. Runtime owner coordination is required.

## Artifacts and limitations

- Server journal: `logs/2026-11-24/20261124_121332/server-01/events.jsonl`.
- Client command route: `logs/2026-11-24/20261124_150432/client-01`.
- Earlier host/process sample is separate and several minutes earlier: `.dev/perf-captures/cpu-bloodtest-pair-20261124T1513Z.csv`.
- Probe values are 1/64 scaled thread-cycle estimates; samples are sparse, scopes may overlap, and reported source lines indicate probe placement rather than exact instruction lines.
- No optimization candidate was changed. The CPU issue now ranks broader attribution, direct measurement of the duplicate config poll, and a generation-aligned 67-NPC target profile as the next experiments.
