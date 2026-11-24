# Gen609 CPU pressure and client stall evidence

Date: 2026-11-24 UTC  
Agent: `root-resource-loop`  
Issues: `issue-ed3ac775283d`, `issue-136d4553ff92`

## User direction

The user asked that this goal center on code changes and repeated runtime
before/after measurements, with CPU/resource reduction as the main objective.
The existing active goal already states this loop and the 240 FPS client,
60 Hz server, gameplay-correctness, and low-resource targets. This record
clarifies the execution gate: each game-DLL build requested for this CPU goal
must carry one named CPU-focused source change and matched pre/post evidence.
Unrelated feature builds by other owners are recorded as context, not counted as
CPU optimization iterations.

## Direct observations

- Build attempt `game-dll-20261124T194125987951Z-4d7bb0` ran from
  2026-11-24 19:41:25.988Z to 19:54:47.033Z, 801.045 seconds. Its changed
  paths were `src/entities/player-loader.cpp`,
  `src/physics/physical-entity.cpp`, and `src/entities/player.h`; none was a
  CPU-focused change from this task.
- Its automatic resource sidecar recorded 685 one-second samples over 12
  logical CPUs. Mean host CPU was 99.926%, peak 100%; mean available RAM was
  1,395.7 MiB, minimum 62.7 MiB. Sampler thread CPU was 11.875 seconds over
  the 801-second attempt (about 0.124% of whole-host capacity). The sidecar
  lacks a matched pre-build baseline and host commit-charge/limit.
- During the sampled interval, run `20261124_193611` was Bloodtest on
  `slaughterhouse1`, with one NPC and one player near the end. Server PID 4372
  averaged 10.588% whole-host CPU (about 127.1% of one logical CPU); client
  PID 30608 averaged 24.864% whole-host CPU (about 298.4% of one logical CPU).
  Pair mean was 35.452% whole-host. The client mode was not independently
  verified by the sidecar, so workload alignment is incomplete.
- Client journal at 19:54:42.979Z reported 145.15 FPS; all 146 sampled frames
  exceeded the 4.1667 ms target, with p95 9.093 ms and max 11.118 ms.
- At 19:54:52.040Z the client watchdog reported no completed frame and
  progress at `Replay`, under `engineTickSimulate` at
  `src/engine/engine-tick.cpp:218`. The captured native stack was
  `NtCreateFile -> GetDriveTypeW -> CreateFileA`. These native frames do not
  resolve the originating C++ function. The client later exited; only server
  PID 4372 remained, outside a registered pair.
- Server journal subsequently reported host DLL generation 609 while NPC
  `module_generation` remained 607. This does not prove the new NPC behavior
  module was active.

## Interpretation and limits

The direct process sample supports the user's observation that MiMITA can use a
material share of host CPU, and this run missed the client frame-time target.
The host was also saturated and low on available RAM, with active compiler
processes. There is no matched no-build baseline, no same-generation verified
client/server pair, and no CPU-by-function profile, so this does not prove that
the build caused the game load or identify a source-level CPU culprit. The
Replay watchdog is a reproducible lead to investigate, not a diagnosis.

No runtime optimization was changed or tested in this session. The next
CPU-owned build should be held until a valid paired workload and profiler path
are available, then include exactly one named candidate and matched before /
after CPU, frame-time, tick, and gameplay evidence. Existing frame wall-time
scopes and the bounded CPU probe are useful guides, but no optimization should
be selected from wall-time scopes alone.

## Artifacts

- `.dev/build-timings/game-dll-20261124T194125987951Z-4d7bb0.json`
- `.dev/build-timings/game-dll-20261124T194125987951Z-4d7bb0.resource.json`
- `.dev/build-timings/game-dll-20261124T194125987951Z-4d7bb0.resource.jsonl`
- `logs/2026-11-24/20261124_193611/server-01/events.jsonl`
- `logs/2026-11-24/20261124_193611/client-01/events.jsonl`

## Verification

Read-only sidecar and journal parsing only. No source code changed, no new
tests were run, and no build, launch, process restart, or runtime command was
requested by this task.

## Follow-up runtime lead

- A separate run, `20261124_195553`, was still writing journals at 20:11Z for
  server PID 13680 and client PID 35812, Bloodtest on `slaughterhouse1`, one
  NPC and one player. Root dev-loop did not register this pair; another server
  PID 4372 was also alive. No process sample or command was issued against
  either run.
- A client `performance.frame-slice` at 20:08:03.788Z covered 15 frames in
  202.682 ms. All 15 frames exceeded 4.1667 ms. `Setup::PlayerHotReload`
  totaled 22.3915 ms with a 22.3808 ms maximum, roughly 11.05% of that wall
  window; its parent `Setup` totaled 29.6843 ms. The scope is placed at
  `src/engine/engine-tick-setup.cpp:112` around
  `updatePlayerProceduralHotReload`, implemented at
  `src/entities/player-config.cpp:152`.
- That function rate-limits itself to 250 ms, then checks file existence,
  modification time, and `animationConfigChanged()`. One later 20:09 frame
  slice did not repeat the large PlayerHotReload scope. The 20:08 and 20:09
  windows overlapped a separate generation-613 build which later failed during
  link, so these are not clean no-build measurements. The 22 ms scope is wall
  time and could reflect I/O or host scheduling; it is not CPU-cycle proof.
- The later client window at 20:09:19.063Z reported 36 frames over 200.196 ms,
  30 over budget, p95 8.699 ms/max 9.685 ms. Wall scopes included Networking
  49.844 ms and Replay 20.498 ms. Server timing near 20:08 reported 58.82 Hz,
  mean tick 3.438 ms, p95 14.726 ms, and max 88.345 ms. These scopes are leads,
  not function CPU shares.
- Proposed falsifiable first CPU code candidate, not yet edited: change only
  the player-procedural config polling interval from 250 ms to 1000 ms in
  `src/entities/player-config.cpp`. Hypothesis: fewer filesystem checks reduce
  polling CPU and long poll outliers; tradeoff: config edits may take up to one
  second to apply. Test only after a matched no-build baseline on a released
  connected pair, with process CPU, this scope, frame distribution, and config
  reload latency measured before and after. Revert if benefit is negligible or
  hot-reload behavior regresses.

No code change or runtime optimization has been tested. Messages requesting a
runtime release and profiler ownership clarification are recorded in the
coordination issue discussion.
