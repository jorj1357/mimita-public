# Payload on Mimita Pipes crash fix follow-up

Date: 2026-11-28
Issue: issue-c9b65fb9948e

## Root cause and fix reviewed

The exact crash was independently symbolized by the issue owner and a second reviewer: a detached `player-model-load` worker used a raw `PendingPlayerModel*` owned by `Player::mPendingModel`. Player teardown during join-time actor/scene churn freed the struct while tinygltf was reading `resolvedPath`, producing the observed access violation in UTF-8 path conversion. The shared checkout contains the fix in `src/entities/player-loader.cpp`: the detached job captures the owning `shared_ptr`; invalid empty/missing model paths also emit a structured diagnostic with condition, expected/actual path, owner, and suggestion.

The root dev-loop published game DLL generation 809 from approved request `build-request-59479dc50ebe`, attempt `game-dll-20261128T063600310452Z-db408b`. Postmortem: 47.458s total, 29.913s compile, 5.989s link, 0.352s publish, two TUs compiled; changed source is `src/entities/player-loader.cpp`. Build success confirms the fix compiled, not runtime correctness.

## Runtime proof status

The pre-fix reproduction on generation 808 crashed in the model-load worker. A post-fix attempt, run `20261128_063715`, used an incorrect profile/launch combination: launch mode 15 sends a local `start payload` command to the joining client and the TDM profile injects conflicting server configuration. The client did not complete join; that run is inconclusive, not a pass. A compatible test should use a Payload base profile and a network client without a local startup command. Proposed harness details are in issue-c9b65fb9948e and remain unvalidated.

I did not edit the source file or start another build/run. The root cause fix was owned by `rocket-league-aerial`; I independently reviewed the diff, artifact identity, and failed test configuration.

## Resource admission evidence

Observed UTC 2026-11-28T06:46:11.234Z: total CPU 96.1%, and again 100% at 06:49:01.827Z. Twelve logical CPUs; 4,123.5 MiB available of 16,024.5 MiB. Three-second process deltas at ~06:46Z: PID 16336 `mimita` 203% per logical CPU (16.92% whole host), RSS 460.8 MiB; PID 17456 `mimita` 186% (15.50%), RSS 225.7 MiB; PID 12360 `mimita` 48% (4.00%), RSS 24.9 MiB; PID 18956 `FL64.exe` 104% (8.67%), RSS 601.6 MiB. Pages Input/sec sampled 63. PID 17456 is an unregistered Payload/mimita-pipes server without a journal. PID 16336 has conflicting manifest role and command line, so ownership/workload is unknown. PID 12360 is the root dev-loop arena2 Vehicle Free Play server, roster 1, no NPCs; a new client later appeared on its run. No build was active. These observations are recorded on the existing host-load issue issue-2a5c5da53565; they do not establish cause of total host load.

I preserved all live processes. Starting a further two-process test while the host is at 100% would add load and produce unreliable evidence. Runtime acceptance remains open: run a corrected pinned generation-809 Payload/mimita-pipes network join after CPU pressure clears, then inspect paired journals for accepted join, model loads, active mode and survival for at least 60 seconds with no access-violation crash.
