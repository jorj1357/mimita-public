# Live build stage timer and resource audit

Date: 2026-11-21

Issue: [issue-136d4553ff92](../issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md)

## What looked frozen

The two supplied screenshots show different phases. The first shows
`MANIFEST SNAPSHOT` with `5.4s elapsed`; the second shows `SOURCE SNAPSHOT`
with `143.4s elapsed`. Those numbers were not live phase durations. The
dashboard used `progress.elapsed_s`, which is the whole-attempt age last saved
at the most recent stage boundary. Before the worker starts, the root build
does not refresh that progress file during long synchronous stages.

In the current attempt, the dashboard initially kept showing the saved
`35.4s` attempt age while manifest snapshot ran. Reading the stage start from
the timing log gave the actual phase start, and the revised dashboard showed
both a live attempt clock and a live phase clock. The visible refreshed view
showed `ATTEMPT 06:55.6` and `PHASE MANIFEST SNAPSHOT 06:20.2`; it also showed
`NO PROGRESS 380s`. The clock was advancing while the progress file and timing
log had no new rows. That means the phase was slow or blocked, not that the
dashboard refresh itself was frozen.

## Captured attempt

Root dev-loop attempt `game-dll-20261121T222152398558Z-ad2cd0` was already
active and was not started or changed by this investigation. It builds source
generation 17 while run `20261121_215237` is live. Its observed timing log:

| Stage | Wall time | Evidence |
|---|---:|---|
| `codegen:resident_state_codegen.py` | 8.539 s | stage start 22:21:53.928Z, completion 22:22:02.467Z |
| `codegen:packet_registry_codegen.py` | 18.114 s | stage start 22:22:06.065Z, completion 22:22:24.179Z |
| `manifest_snapshot` | 409.415 s | start 22:22:27.829Z, completion 22:29:20.409Z; 1,008 C++ source entries; zero retries |
| `manifest_validation` | 0.289 s | 1,008 source entries |
| `source_snapshot` | 111.612 s | start 22:29:41.343Z, completion 22:31:38.870Z |
| `changed_source_preflight` | 148.014 s wall | three changed `.cpp` units were compiled |
| `object_cache_scan` | 7.403 s | 642 candidates, 14 compiled total, 628 skipped |
| worker `compile` phase | 77.711 s wall | the 11 generated module wrappers compiled in parallel |
| GNU linker | 30.683 s | 642 object inputs, 480,309,960 object bytes, 665 linker arguments |
| `publish` | 8.633 s | includes a 6.246 s external dependency manifest substep |

The end-to-end `build.completed` event reports 903.413 seconds (15m03s) and
published generation 436 successfully. A separate dev-loop result field
reports 886.724 seconds, a 16.689-second mismatch that remains unexplained.
The end-to-end event includes the 586.465 seconds elapsed by the end of source
snapshot, then the later worker/link/publish stages. A further 32.855 seconds
passed between `source_snapshot` completion (22:31:38.870Z) and worker input
validation start (22:32:11.725Z). Code after the snapshot that is not split
into timers includes snapshot pruning, manifest diff/change-report writing,
progress/context JSON writes, build-slot acquisition, and worker startup; the
exact owner of those 32.855 seconds is unknown. This is build evidence, not
proof that generation 436 activated in the running game. The run journal has
no `game.generation.activated` event for generation 436.

After the two snapshots, the next observed worker phase was
`changed_source_preflight`, compiling three translation units:
`src/gamemode/map-config.cpp`, `src/mapeditor/map-editor-commands.cpp`, and
`src/mapeditor/map-editor.cpp`. At the last observation, that phase had run
for 54.7 seconds, with all three translation units active. It then completed
in 148.014 seconds. The complete attempt record is now available, although it
belongs to another agent's automatically triggered source edit, not a
controlled one-file benchmark.

The completed snapshot metadata reported 2,899 hardlinked files and zero
copies in `.dev/build-snapshots/game-dll-20261121T222152398558Z-ad2cd0/source`.
The manifest contained 1,008 `.cpp` entries and 1,217 header entries; the
source hash cache contained 2,225 entries. The directory walk counted 2,900
files including `.snapshot.json`. These are counts, not evidence of cache hit
rate or bytes hashed: those counters are currently missing.

### Why the build compiled eleven additional wrappers

The 11 extra units were not obsolete legacy NPC modules. They were active
module wrappers, and the timing log gives the same invalidation reason for all
11: `dependency newer than object: .../build/game/identity/module-identity.h`.
The wrappers were `map-geometry`, `navigation-policy`, `network-policy`,
`npc-behavior`, `npc-perception`, `mapeditor`, `world-rendering`,
`chunk-collision`, `actor-contact-policy`, `actor-rendering`, and
`infinite-worldgen`. Each included the generated identity header. The three
edited C++ units and those 11 wrappers account for all 14 compiled units;
the other 628 candidates were skipped.

This fan-out is documented in
`docs/architecture/live-development/incremental-build-and-header-fanout.md`.
The generated header contains `MIMITA_MODULE_GENERATION` and the full source
revision. Both values change on a new generation/revision and are compiled
into wrapper code. Merely preserving the old header timestamp would be
incorrect. The proposed, unimplemented direction is to keep compile-time
identity for separate module DLLs but provide folded whole-game module
generation/revision through host runtime identity services. That needs an ABI
and runtime proof; it is a real architectural change, not a safe cache toggle.

The object scan itself was only 7.403 seconds in this attempt. Its dependency
prepass was 5.734 seconds, dependency index load/save were 0.790/0.299
seconds, and 639 dependency checks averaged 1.124 ms. The 160k-token outlier
from the earlier attempt is not representative of this run; the current
attempt's full object scan is still above the one-second goal, but its
largest measured substep is the dependency prepass rather than the per-object
freshness check.

The worker summary reports 903.413 seconds at the attempt level but 886.724
seconds in its nested `worker_metrics.total_seconds`; the dev-loop result event
uses the latter. Along with the 32.855-second pre-worker gap, this is a
specific timing-reconciliation defect to investigate. Keep root attempt wall
time, worker process wall time, and stage/phase sums as separate metrics until
their start/end boundaries are reconciled.

## MiMITA CPU observation and likely levers

After the DLL attempt had completed, run `20261121_224058` was live in Payload
mode with one local-server NPC. A 15-second process sample recorded 12 logical
processors, 85.3% aggregate system CPU, and three MiMITA processes:

| PID | Role evidence | CPU as share of full 12-logical-CPU machine |
|---:|---|---:|
| 24192 | dev-loop client | 20.27% |
| 27816 | dev-loop local server | 10.53% |
| 20908 | child server on `badhouse` / `jugger_horde`, NPC=1 | 12.56% |

All three were normal priority class. Together they used about 43.36% of total
machine capacity while the build was no longer active; other applications
accounted for much of the remaining load. The third child server was not the
server PID shown in dev-loop's active-run status, so role-aware process
inventory matters before calling this a clean one-client/one-server result.

The client journal confirms `target_fps_max=999`, `target_frame_ms=1.001ms`,
and `vsync=false`; `config/video-settings.json` also sets `maxFrames` to 999.
The `FramePacer` sleeps only if frame work completes before that 1ms target; it
does not spin-wait, but the setting still permits an aggressive render loop.
In the client log, later one-second FPS windows ranged from 36.5 to 71.3 FPS,
with p50 frame times about 13.5-26.8ms and p95 about 18.0-37.6ms, while the
client reported zero NPCs. Other high-cost samples included remote-entity
network processing and player animation. This confirms real game-side CPU
work and a very high configured cap. It does not isolate a single C++ hot
function, and the sample was not a controlled “standing still” condition.

Task Manager's process percentage normally represents share of the whole
machine in this measurement. On a 12-logical-CPU machine, 1% total-system CPU
is only 0.12 logical-CPU equivalents. A client at 20.27% machine CPU would
need roughly a 20x reduction to reach that bar; the three-process sample would
need about 43x less total MiMITA CPU. Lowering the frame cap is the first
reversible experiment for a genuinely idle/background client, while preserving
VSync-off and the fixed 60Hz server simulation. Do not promise <=1% for active
visible gameplay until measured; a server that must keep authoritative ticks
and a client that renders at 240 FPS have a different floor from a paused,
minimized client.

## Why these stages are slow, and what remains unknown

The current code locates both long operations precisely but does not split
their costs:

- `devscripts/dev_loop/game_build.py:627-652` wraps `build_manifest(...)`,
  writes the hash-cache JSON, and calls `_update_source_mirror(...)` inside one
  `manifest_snapshot` timer. The timing record only reports total duration,
  source count, and retry count.
- `devscripts/source_manifest.py:102-114` walks the repository tree for C++
  files; `:116-130` recursively enumerates headers; `:237-296` resolves C++
  ownership, stats each path, hashes files when the size/mtime cache stamp
  misses, and assembles the revision. No enumeration time, hash-cache hit or
  miss count, or byte count is emitted.
- `devscripts/dev_loop/game_build.py:141-180` updates/verifies the persistent
  source mirror, which is included in the same manifest timer and has no
  per-path/copy/verification measurements.
- `devscripts/dev_loop/game_build.py:232-263` removes the attempt snapshot
  destination, recursively walks `src`, `include`, and `external`, then creates
  a hardlink per file (copy fallback on error). The stage only reports total
  elapsed time; it omits per-tree counts, link/copy outcomes, bytes, and
  directory-walk time.

The evidence shows that file enumeration/mirror work and snapshot linking are
large candidates, but it does not yet prove which sub-operation consumed the
409.415 seconds in `manifest_snapshot`. In particular, the hash-cache hit
rate, hashed bytes, source-mirror delta size, and time in owner resolution are
unknown. The 111.612-second snapshot time is measured, but we do not yet know
whether the dominant cost was traversal, directory creation, hardlink calls,
or host filesystem/antivirus contention.

## Live PC load and job-count interpretation

During a five-second observation with the live build and three `mimita.exe`
processes present, Windows reported 100% aggregate CPU across 12 logical
processors. The three game processes used about 348.8%, 101.2%, and 55.1% of
one logical processor each. Together that is about 505% of one core, or 42.1%
of total 12-core capacity. The sample cannot attribute the remaining CPU load
to a single process. An earlier eight-second snapshot during this
investigation showed 98.3% system CPU and 2.0 GB available RAM.

`jobs=10` is the compiler worker's maximum concurrent translation units; it
does not cap whole-machine CPU at ten cores and does not apply to manifest or
snapshot work. The captured 409-second manifest and 111-second snapshot are
root pre-worker stages where `jobs` is absent. Process priority can decide
which work runs first under contention, but it cannot make demand disappear:
the data does not justify making the build high priority, and reducing game
process priority would not reduce its own CPU demand when the machine is busy.
Avoiding repeated work and reserving an explicit CPU/memory budget are the
relevant levers. No controlled idle-vs-live comparison was run, so PC pressure
is a plausible contributor, not a proven cause of the exact stage durations.

## Measured optimization sequence

1. **Keep trustworthy clocks and stage boundaries.** The GUI now derives the
   attempt timer from `started_utc`, the phase timer from
   `phase_started_utc` or the phase's timeline start, and a subphase timer from
   `subphase_started_utc`. Keep the no-percentage message for work with no
   defensible denominator. Do not label attempt age as phase elapsed.
2. **Add manifest substage evidence before changing its algorithm.** Record
   wall time for C++ enumeration, header enumeration, owner resolution,
   hash-cache reads/hits/misses, bytes actually hashed, hash-cache write,
   mirror diff, changed/added/removed paths, copy bytes, verification, retry
   count, and free/used memory where cheap. Emit live elapsed and bounded
   counters during long loops; final-only records will identify the owner but
   will not make the dashboard responsive during the stage.
3. **Target a warm manifest under one second with an incremental index.**
   Maintain directory and dependency state while idle; on save, update only
   changed files and affected owners. Preserve a full correctness fallback on
   watcher overflow, new/deleted/renamed files, manifest/config/toolchain
   changes, and periodic audit. Do not trust size/mtime stamps alone as the
   final freshness proof where file-system notifications can be lost.
4. **Target a warm source snapshot under one second.** Measure file/link and
   directory operations first. Reuse an immutable content-addressed tree and
   update only changed paths, or use verified filesystem copy-on-write where
   available. Keep per-attempt source immutability; do not silently replace it
   with mutable links to the working tree.
5. **Remove generation-only wrapper fan-out safely.** The current 11 wrapper
   recompiles are caused by compile-time `MIMITA_MODULE_GENERATION` and full
   revision macros in `module-identity.h`, not by stale NPC behavior modules.
   The architecture note already proposes runtime identity from host services
   for folded whole-game modules while retaining compile-time identity for
   separate module DLLs. Prototype that with an ABI bump and journal proof;
   do not just ignore the header dependency. Then a one-file edit should not
   rebuild the 11 wrappers solely to stamp a new generation.
6. **Make irrelevant generators and wrapper compilation disappear.** Cache
   generated outputs by their actual inputs and run generators only when an
   input fingerprint changes. For a one-file behavior edit, compile one
   translation unit when headers and generated contracts are unchanged.
   Record each wrapper's exact invalidation reason. The goal is zero wrapper
   compiles on an unrelated edit, not merely making eleven compiles somewhat
   faster.
7. **Keep the object cache correct while eliminating full rescans.** Maintain
   depfile metadata incrementally and check only changed inputs/dependents on
   a warm edit. Force invalidation for compiler identity, flags, PCH, generated
   headers, module membership, or toolchain changes. Measure candidates,
   invalidations by reason, and bytes/paths inspected.
8. **Treat link time separately.** A full DLL link creates a new image from
   object files and import libraries; MinGW's current whole-image link is a
   separate cost from compiling one `.cpp`. The earlier attempt record in the
   issue tracker measured 26.179 seconds for a failed link under overlapping
   work. A sub-second full link is not established. Compare linker
   incremental modes/LLD and a function-patch prototype; Blink is potentially
   the function-replacement path but requires a compatible compiler and debug
   symbol workflow, while `cr.h` reloads a complete library and does not
   eliminate source scanning or compile/link. Keep MinGW DLL reload as the
   control and do not switch the active compiler based on assumptions.
9. **Control load with evidence.** Repeat a matched matrix: no game, one live
   client/server, and two clients/servers; same saved source edit and build
   state; capture aggregate/per-process CPU, memory, process priority, disk
   activity, jobs, queue waits, and each stage's wall time. First run at the
   current job cap and normal/below-normal build priority; only then tune an
   adaptive jobs budget. A 10-second end-to-end result requires deleting
   repeated preparation/wrapper/full-link work, not letting a build consume
   every core.

## Acceptance gates for the next experiment

- Warm no-op and single `.cpp` edit both report complete stage totals and no
  unexplained gaps; warm cache/manifest and snapshot work each target <1s.
- A leaf behavior edit compiles only its expected translation unit; wrapper
  count is zero unless a measured dependency changed.
- Link and publish are separately timed; do not claim <1s without a measured
  successful artifact.
- Save-to-first-confirmed-runtime-activation is measured from timestamps in
  the same build attempt and game-instance journal, with p50/p95 across
  repeated trials. Build success alone is not activation proof.
- First target: <=10 seconds to visible behavior with one live client/server.
  Then repeat with two clients/servers and multiple edits under resource
  pressure. Track CPU impact as a separate acceptance metric.
- For CPU work, first measure same-instance 60-second foreground, background,
  and minimized cases with `maxFrames=999` and a lower cap. Report whole-system
  and per-process CPU plus FPS and server tick health; decide separately
  whether <=1% is realistic for idle background operation and visible play.

## Validation and scope

- `python -m py_compile devscripts/dev_loop/gui.py` passed.
- Direct timer-helper checks passed for timestamp parsing, duration formatting,
  and phase timeline fallback.
- `_game_dll_queue_view` was evaluated against the active attempt and returned
  distinct ticking total and manifest-phase clocks.
- The existing dashboard view was closed and reopened through the existing
  keepalive path; the screenshot showed the two timers advancing. The dev-loop
  daemon, running client/server, active build, build configuration, active
  compiler, and game architecture were not changed.
- No new compiler/build request was issued. The captured build is still in
  progress and has not yet supplied compile/link/publish/runtime-activation
  results for this experiment.
