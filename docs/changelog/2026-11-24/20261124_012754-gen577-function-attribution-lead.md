# Gen577 function attribution lead

- Task ID: `work-0164aec8174a`
- Session ID: work-0164aec8174a
- Owner: `resource-profiler`
- Focus: `issue-ed3ac775283d`; gen577 resource and runtime attribution
- Status: Read-only source lead recorded; CPU attribution still pending

## Evidence inspected

- Root dev-loop reports generation577 published, but run metadata is stale: server PID32360 is marked dead and client PID27052 alive.
- Win32 process inspection found client PID27052 and its child server PID19056 alive. The only registered client journal is capped/stale; its latest usable window reports Payload/funworld3, 1 NPC/0 server players, so this is not a verified client/server gameplay pair.
- Gen577 resource summary `.dev/build-timings/game-dll-20261124T010546039497Z-2ea83a.resource.json`: 44 one-second samples, host CPU mean99.715%, peak100%, minimum available RAM228,880,384 bytes; provider alignment unavailable. Source sidecar path `.resource.jsonl`.
- Existing elapsed scopes report `Audio::MusicConfigPoll` 0.862s / 46s and max43.279ms. These are wall-time scopes under saturation, not function CPU shares.
- Source trace: `src/engine/engine-tick.cpp:170` calls `engineTickAudio`; `src/engine/engine-tick-audio.cpp:7` calls `MusicManager::update`; `src/audio/music-manager.cpp:578` performs the update, including `std::filesystem::last_write_time` at lines586–595 each frame iteration.

## Changes

- Appended this candidate and discriminating next measurement to CPU issue `issue-ed3ac775283d`; it is explicitly unproven and does not claim CPU causation.
- Sent the exact run identity/journal limitations and proposed safe next profiling sequence to the DLL latency owner thread.
- No source, runtime, build, or process state changed. No capture was started because the current pair belongs to the active Payload verification task and its journal is stale.

## Next evidence step

After the active runtime owner releases its run, verify one isolated Juggernaut client/server pair, fresh matching journals and DLL generation, then take a bounded host/process baseline and run the existing bounded callsite probe. Only consider a MusicConfigPoll optimization if thread-CPU attribution shows a material cost; verify hot-reload responsiveness and 240 FPS/60 Hz behavior after any change.
