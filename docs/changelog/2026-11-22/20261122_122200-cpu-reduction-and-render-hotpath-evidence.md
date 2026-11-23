# CPU reduction and render hot-path evidence

## Changes

- Changed the `npc.dash` runtime diagnostic in `src/npc/npc.cpp` from an Important event emitted on every successful dash to an opt-in Debug event guarded by `StructuredLogger::shouldLog`. This avoids constructing the per-event JSON payload and synchronously writing/flushing the record when Debug logging is disabled. Gameplay dash behavior is unchanged.
- The file also contains an in-progress FCS movement-metrics change owned by another agent; that work is not attributed to this CPU change.

## Evidence

- In server run `20261122_144609_01`, a 60-second log window contained 1,002 `npc.dash` Important events from 54 actors (about 16.7 events/second). `hostLogEvent` forwards each DLL event to `StructuredLogger::writeEvent`; Important events take the flush path.
- A five-second process sample near 2026-11-22 16:46 UTC on 12 logical CPUs measured client PID 30076 at 20.406 CPU-seconds (34.0% of total machine capacity) and server PID 4540 at 2.047 CPU-seconds (3.4%). A second earlier server PID was no longer present.
- Client frame slices at 16:52:27–28 UTC reported 46–60 frames per approximately 0.2-second slice, 3.2–4.5 ms average frame time, and 80–97 ms of Rendering work per slice. `Rendering::Shadows` accounted for about 25–31 ms; `Rendering::Skybox::Reload` 18–30 ms; `Setup::ConfigPolling` 8–23 ms. The render owner calls `Skybox::pollReload` every frame; that function checks shader reloads and calls `std::filesystem::last_write_time` for its config. A 250 ms polling throttle was examined, then deferred to avoid queueing another full build while generation 503 was still compiling.
- The same run’s launch metadata says `npcs=1`, but server telemetry reports 66 living NPCs. At 16:52:25–28 UTC, 66-NPC samples reported about 424–551 ms in NPC AI and 338–804 ms in physical contacts per server tick window. This run does not prove a one-NPC workload; mode roster selection must be reconciled before comparing server CPU.
- Root dev-loop generation 503 used 642 sources, 10 jobs, and an unseeded object cache. The `src/npc/npc.cpp` translation unit took 589.518 seconds (GCC parsing 379.99 seconds, 504 MB GGC; template instantiation 180.76 seconds, 341 MB GGC). Other observed units took 485.734 seconds (`src/combat/weapon-rocket-launcher.cpp`) and 275.021 seconds (`src/camera/camera-commands.cpp`). At 17:03:39 UTC the log showed progress through unit 526 of 642. This evidence was appended to the CPU and build-friction issue discussions and shared with the NPC metrics owner.

## Validation state

- Source change: present in the shared working tree.
- Build: generation 503 failed its changed-source preflight on `src/npc/npc.cpp` after 65.67 seconds when I edited that compiler input while it was being built. This was a source-change collision caused by my edit, not a compiler diagnostic. The dev-loop is now waiting for the single EXE build; the next DLL build has not completed. The existing user-owned client/server were kept alive.
- Runtime: not yet proven on a fresh DLL. The existing run uses the previous artifact, so no CPU reduction is claimed.
- WPR profiling previously failed because Windows denied the system-performance profiling policy; source-level CPU stack attribution remains unavailable without that permission.
