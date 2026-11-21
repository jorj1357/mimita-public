# Dev-loop CPU follow-up

## Work completed

- Removed the full `last_snapshot` dictionary copy and full-map diff from every ordinary Windows watcher wake in `devscripts/dev-loop.py`.
- The normal path now compares the small fixed `WATCH_FILES` set and processes exact `ReadDirectoryChangesW` events. Overflow recovery still performs a full snapshot reconciliation, preserving correctness.
- Recorded process and per-thread CPU evidence on the currently running Payload session in issues 136 and `issue-ed3ac775283d`.

## Verification and evidence

- `python -m py_compile devscripts/dev-loop.py` passed.
- `git diff --check -- devscripts/dev-loop.py` passed; only Git's expected LF-to-CRLF notice appeared.
- An 8-second `psutil` sample observed 67.6% host aggregate CPU. The dev-loop daemon used about 42.6% of one logical core: thread deltas were 32.2% and 10.4%. The dashboard's sampled active threads totaled about 7.6%. The runtime was Payload, not Juggernaut.
- The previous exact-path temporary watcher probe measured filesystem notification delivery, not daemon CPU or save-to-game activation. The new watcher code remains unloaded in the running daemon, so there is no after-change CPU comparison.

## Limits and next evidence needed

- `psutil` thread CPU deltas do not identify Python call stacks. The leading thread is consistent with the daemon main loop and another with the watcher, but exact function attribution needs bounded in-process stage timing or a CPU stack profiler.
- Relevant source locations to instrument next are `DevLoop.run()` (`devscripts/dev-loop.py`, around lines 5647–5680) and `DevLoop.watch()` (around lines 3552–3635). Current logs contain no slow dashboard-stage records matching this sample.
- No dev-loop restart, Juggernaut launch, build, or additional game process was started. Preserve the current shared runtime until a safe pinned stress run can be scheduled.
- Active gameplay at a 999 FPS client cap cannot reasonably target 0% CPU. Measure idle/background and active-game CPU separately; the <=1% target applies only to an explicitly idle/background scenario.
- This watcher cleanup is a small CPU-path reduction, not proof of meaningful whole-machine improvement. DLL manifest enumeration and hardlink snapshot walks remain separate unresolved stages.
