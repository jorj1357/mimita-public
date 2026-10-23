# Juggernaut replica spike and late-run jitter evidence

## Source changes

- `src/debug/structured-log.cpp`: include the structured entry's `message` in JSON fields. This makes the detailed `PERFORMANCE_SPIKE` breakdown visible in `events.jsonl` instead of emitting an event with empty fields.
- `src/network/multiplayer-tick.cpp`: add perf scopes around NPC username assignment, avatar identity selection, avatar-name assignment, and avatar console logging during snapshot replica creation.
- `src/npc/npc-avatar.cpp`: cache the avatar choice list and refresh it at most once per second. This prevents a snapshot that creates many NPCs from rescanning avatar folders and parsing avatar JSON once for every NPC.
- `src/perf/perf-spike.h`: make perf-scope variable names expand through `__COUNTER__`, allowing multiple scopes in one function without duplicate declarations.

## Evidence

- Build 2869 run `20261023_040531` isolated `NpcAvatarIdentity` as a severe replica-creation cost: one sampled frame spent about 912 ms and 373 MB of allocations in that scope while creating NPC replicas. The code path selects an avatar and previously called `AvatarSystem::listAvatars()` for each new NPC.
- Build 2870 includes the avatar-list cache. Its measured `NpcAvatar::RefreshChoices` operation was about 12 ms and 1.25 MB for a refresh, rather than repeating the directory scan for each new replica. This supports the cache as a targeted fix for the replica burst; it does not establish that the overall frame-time target was met.
- Build 2870 latest run: `logs/2026-10-23/20261023_041034/events.jsonl`, headless Juggernaut, exact executable `.dev/builds/2870/mimita.exe`. At 49.76 seconds, the last client journal record was a `PERFORMANCE_SPIKE` for frame 9001: 25.489 ms, with 15.659 ms in replay, 14.616 ms in simulation (including 3.307 ms in corpse updates), and 2.654 ms in networking. This is far above the 4.167 ms budget for 240 FPS.
- The same frame's watchdog reported 24 active corpses, about 1,015.6 ms of main-thread CPU in a one-second sample, and 84% system memory load. These measurements show significant main-thread load and corpse cost, but do not by themselves prove that corpses caused the later journal cutoff.
- Client and server journal activity stopped at approximately the same wall-clock second while the client process remained alive. A later `versioninfo` request was not delivered/completed. The journal did not capture a CPU stack at that point, so the reason the runtime stopped servicing work remains unknown.
- The run ended before a complete 60-second recording. There is no acceptance evidence for consistent 240 FPS; the available late-run frame data demonstrates misses.

## Next evidence needed

Capture a native thread stack and determine whether the client/server are blocked on a shared logging/file lock, stalled in a runtime loop, or affected by another host/process condition when the journal goes quiet. Then compare a full-minute Juggernaut run with and without the accumulating corpse workload. Keep the NPC avatar cache as a targeted replica-burst optimization; do not claim it fixes late-run jitter or the 240 FPS target.

## Validation

- Dev Loop build 2870 published successfully from source generation 9.
- Dev Loop headless Juggernaut run was started and its journal was inspected. It stopped producing client/server journal events at about 50 seconds while the process remained alive; the Dev Loop was then stopped cleanly.
- The strict 240 FPS target and a complete 60-second zero-miss run remain unverified and unmet by this evidence.
- No commit was created.
