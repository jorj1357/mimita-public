# Move build resource discovery off the serial startup path

## Finding

The generation 642 attempt `game-dll-20261125T084339926983Z-be8067` spent
3.953 seconds in synchronous resource-sampler setup before `build.started`.
The sampler's initial process inventory was responsible for most of that
measured setup. This is an instrumentation delay on the request-to-worker
path; it does not explain later compile/link time and does not establish a net
edit-to-live improvement.

## Change

`BuildResourceSampler.start()` now emits an immediate host memory sample and
labels CPU as `baseline_only`, then returns while process discovery runs on its
own background thread. Periodic host CPU/memory sampling continues on its
configured cadence during the inventory. Once process identities are available,
runtime-context discovery is queued on a separate background thread rather
than blocking a host sample. Both attribution paths expose readiness and
in-progress status and emit `resource.process_attribution_initialized` or
`resource.runtime_context_initialized` records. Final sampling reuses cached
process identities and runtime context rather than starting another synchronous
full scan. Shutdown waits at most one second total; if a worker is still
active, the summary reports a partial capture and skipped final sample. A
closed-sidecar guard prevents late thread writes after the completion event.

## Validation

- `python -m unittest devscripts.quality.test_build_resource_sampler`: 18/18
  passed, including host sample cadence during blocked process/runtime scans,
  short-attempt cleanup, and explicit partial status on inventory failure.
- `python -m py_compile devscripts/dev_loop/build_resource_sampler.py
  devscripts/quality/test_build_resource_sampler.py`: passed.
- `git diff --check` for both changed Python files: passed.
- No game build, daemon reload, or runtime restart was performed. The root
  daemon has not loaded this edit, and no real build has measured its net
  effect yet.

## Coordination and next proof

The active Juggernaut A/B run is held by `cpu-a-b-verification`; it was left
untouched. At 09:21Z root status reported `starting` with no registered runtime
and its previously reported daemon PID20956 absent, while client/server PIDs
7452/10644 remained alive and both journals were still advancing at09:21:32Z.
I notified the run owner and did not attempt recovery or process control. The
`cpu-resource-optimization` agent requested reuse but was told to coordinate
with that owner before stopping or relaunching. A correction was appended to
issue `issue-ed3ac775283d`:
`npc.phase-window.module_generation` is the NPC behavior API module identity,
not the whole game DLL generation. Gen642 activation on both existing client
and server remains proven; the field mismatch alone does not block testing the
compiled `npc-combat.cpp` change.

After the CPU experiment owner releases the run, coordinate a safe daemon-only
reload, then use the next root-dev-loop attempt to compare sampler setup time,
attempt-to-`build.started`, process-discovery wall/CPU, sample cadence,
resource completeness, total build duration, and runtime activation. Discovery
now overlaps build work, so retain the change only if total attempt-to-live
latency improves without unacceptable observer CPU or telemetry gaps. The
source already enforces an 18-minute whole-attempt timeout; live daemon adoption
of that existing timeout was not verified in this session.
