# Build time and cold-build frequency

Time created: 2026-11-08T22:45:00Z
Time last updated: 2026-11-09T19:31:54Z

Status: MITIGATION COMMITTED — matched full-rebuild timing and causal attribution remain open

Related specifications:
- `docs/architecture/live-development/module-migration-plan.md`
- `docs/architecture/live-development/self-contained-workspace.md`
- `docs/specs/hotreload/hotreload.md`
- `docs/specs/dev-loop/agent-lanes-build-authority.md`

Related changelog:
- `docs/changelog/2026-11-08/20261108_202357-lane-build-ledger-and-module-private-headers.md`
- `docs/changelog/2026-11-09/20261109_182915-build-speed-regression-gold-reference.md`
- `docs/changelog/2026-11-09/20261109_193154-build-cache-lane-regression.md`

---

## Regression Occurrence 1

### Observed

Date: 2026-11-08

A lane host rebuild took roughly **20 minutes** and was still compiling. Earlier
attempts took about 26 minutes before failing. The build was a 588-file debug
rebuild after shared-header changes.

Measured contributing facts:

- `map_common.h`: **276** dependent translation units.
- `map-authored-geometry.h` / `map-config.h`: about 61 dependents.
- Debug flags: `-Og -g1`.
- Parallel compilers: 4–8, depending on lane configuration.
- Free physical memory dropped to roughly **0.6–1.1 GB**.
- Two legacy outside lanes were compiling at the same time because each had its
  own scheduler root.
- The build log is written only when the build completes, so the dashboard can
  look frozen while compilers are active.

### Expected Behavior

- Normal module/private-header/config changes: **no EXE build**.
- Ordinary incremental host changes: seconds, not tens of minutes.
- Cold/warm boundary build target: **2–3 minutes**.
- Ideal: effectively 0 seconds for normal work, with a cold EXE boundary only
  for kernel/process/device/transport/packet-schema/loader/public-ABI changes.

### Actual Behavior

- A wide-header change forces a large fraction of the 588 TUs to recompile.
- Multiple lanes can build simultaneously because legacy lanes use separate
  scheduler roots.
- Memory pressure turns 4–8 parallel debug compilers into tens of minutes.
- Build-time history was not recorded in a machine-readable tracker.

### Confirmed Cause

1. Wide-header fanout, especially `map_common.h`.
2. Scheduler root divergence across legacy lanes.
3. Too few compiler jobs and low free memory.
4. No persistent build-time/cold-build counter.

### Corrected Code / Mitigation

Implemented in the lane:

- OS-level single-EXE-build mutex (`Local\MiMITA-EXE-Build`) in
  `devscripts/build_exclusive.py`, acquired by `build.py`.
- Shared coordinator scheduler root resolution in `devscripts/build_scheduler.py`.
- `MIMITA_COORDINATOR_ROOT` propagation from `agent-lane.py`.
- Lane dev-loop scheduler-root marker and restart on mismatch.
- Default EXE jobs raised to 8; maximum 12.
- GLB authored v2 state moved to the narrow
  `src/map/map-glb-authored.h` sidecar instead of `map_common.h`.
- Build metrics appended to `build/build-metrics.jsonl` and latest timing in
  `build/build-timing.json`:
  - elapsed seconds
  - compiled/skipped counts
  - cold-build flag
  - jobs
  - lane
  - scheduler root
  - link reason

### Proof Required

- Two lanes attempt a build; only one EXE build compiles; the other waits.
- `build/build-metrics.jsonl` records every successful/no-op build.
- A GLB-authored edit recompiles only the narrow sidecar consumers, not 276 TUs.
- A warm incremental host build target: under 2–3 minutes.
- No-op: under a few seconds and no new artifact.
- Normal module/private-header/config edit: no EXE artifact change.

## Build Time Tracker

Machine-readable history lives at `build/build-metrics.jsonl` (gitignored).
Each line records `elapsed_s`, `cold_build`, `compiled`, `jobs`, `lane`,
`scheduler_root`, and `result`.

Human summary:
- 2026-11-08: ~20 minutes (slow, unacceptable).
- Target: 2–3 minutes for a warm boundary build.
- Ideal: effectively 0 seconds for normal work; cold EXE builds only for
  genuine kernel/ABI/process boundaries.

---

## Regression Occurrence 2 — High-priority, full-processor policy mismatch

### Report and first diagnosis

Date: 2026-11-09

The user reported another build taking about 25 minutes and initially wondered
whether the relevant specification was unclear or hidden behavior was
responsible. Inspection found a concrete build-policy mismatch: before commit
`b7516a78`, the canonical build defaulted to at most four compiler jobs on this
12-logical-processor machine, and the dev-loop launched it with
`BELOW_NORMAL_PRIORITY_CLASS`. The lane build configuration also imposed a
four-job cap. These settings were real and contrary to the user's requested
throughput. They are a plausible contributor to elapsed time, but the available
evidence does not isolate how much time they cost.

### Observed build and status evidence

- The lane's `build.py` process, PID 33988, started at 2026-11-09 11:56:35 EST.
- The pasted live investigation showed object files increasing from 162 to 169
  between 12:22:49 and 12:24:53 EST while the build process remained alive.
  This ruled out a simple frozen compiler at that time.
- The lane later reported `building` at 2,688 seconds with
  `src\entities\player-avatar-animations.cpp` as its last compiler file. At a
  later inspection, the scheduler slot was free and no matching build process
  remained, but the lane state had no final success/failure or artifact. Treat
  that lane attempt's outcome as unknown; its stale `building` status is not
  proof that the compiler was still active.
- A separate main dev-loop build published artifact
  `.dev/builds/2026-11-09T18-11-10.953010Z/mimita.exe` with build ID
  `2026-11-09T18:11:10.953010Z:f538b96de6225c213ac8bbe8dd672e9479074157eb8c0f1048b33ad7ad73a6c6`.
  It compiled 1 source, skipped 622, and reported 33.66 seconds. This is strong
  evidence that incremental builds work quickly for a one-file change; it is
  not a like-for-like full-rebuild comparison and does not establish that the
  new policy made full rebuilds fast.
- The user reported feeling back in a good state after working with DeepSeek
  V4.1 Flash and seeing it build the EXE and run tests. The integration
  changelog confirms the executable build and multiple runtime attempts, while
  also recording unresolved mode-start/map-sync cases and server-tick stalls.
  The user's model/workflow assessment is human context, not a technical cause.

### Cause classification

Confirmed implementation defect: the default build policy used fewer than all
logical processors and below-normal process priority. Corrected commits are
`b7516a78` (all logical processors and High priority) and `ed8787fe` (pin High
priority and reduce the memory wait threshold to 16 MB).

Still unproven: that the policy mismatch alone caused the 25-minute duration.
Full recompilation is selected by source/dependency invalidation, such as a
wide header change; priority and job count affect how quickly eligible work
runs. No matched cold-build comparison with identical source snapshot, cache,
toolchain, memory conditions, and compiled-unit count has isolated the speedup.
Do not record “low priority caused the full rebuild” as fact. The build-time
specification requires phase and per-translation-unit evidence before assigning
the dominant cause.

### Corrected direction and remaining proof

- Keep the persistent dev-loop as the EXE build authority. Agents edit and
  observe; they do not launch `build.py` or a compiler.
- Keep one machine-wide active EXE build slot. Other requests wait rather than
  compiling at the same time. A waiting `build.py` process can exist in the
  current implementation; distinguish a queued process from active compiler
  work when claiming “one process at a time.”
- The active build uses all logical processors and Windows `HIGH_PRIORITY_CLASS`
  (the highest class below real-time). Do not change it back to a four-job cap
  or below-normal priority without a measured, deliberate policy review.
- Keep the dev-loop open so edits are coalesced into incremental builds. Treat
  cold EXE boundaries separately from module changes; modularization and hot
  reload reduce how often full EXE rebuilds are needed, while job count and
  priority reduce elapsed time for work that still requires an EXE build.
- For a causal speed claim, compare cold/warm runs with matching source,
  toolchain, cache, memory conditions, and compiled/skipped counts. Capture
  scheduler wait, memory wait, PCH, per-file compile, link, and publish time.
  The 33.66-second one-file incremental build is a useful baseline, not the
  full-rebuild acceptance result.

Related reference: `docs/gold/2026-11-09-build-speed-and-diagnosis-reference.md`.

---

## Regression Occurrence 3 — Cold lane build, zero ccache hits, stale file status

### Report and evidence

Date: 2026-11-09

The user supplied a build trace beginning at `2026-11-09T18:52:01Z`. It
reported a successful build after `699.798s` (11m40s), with 624 translation
units compiled, none skipped, `cold_build=true`, and
`link_reason=exe_missing_or_zero`. `player-animation.cpp` appeared to remain
the current file in status, but the pasted investigation showed its object was
written about three minutes after start and linking began at `19:03:25Z`; the
link completed at `19:03:41Z`. This is evidence the build progressed and
finished, not that it hung on that translation unit.

The pasted ccache statistics reported 0 hits / 2,466 cacheable requests,
2,466 misses, 35 errors, and 0.3 / 5.0 GiB used. These are user-provided
runtime statistics. The accompanying `ccache -p` output showed an empty
`base_dir` and `hash_dir=true`, but it is not established that this command
used the same environment as the lane worker. The lane's current EXE worker
does explicitly set shared `<coordinator>/.dev/ccache`, lane-root
`CCACHE_BASEDIR`, and `CCACHE_NOHASHDIR=1`; thus “ccache was not linked to EXE
lanes” is not supported by current source. The observed zero hits still need
to be explained against the exact worker environment and cache keys.

Current source review did find a coverage gap: `module-build-worker.py` used
the ccache wrapper but did not configure the coordinator-shared cache or the
same lane-root normalization. That setup is now added before its compiler and
link subprocesses. The stale `.dev/shared-agent-ccache/` path in the
self-contained workspace guide was corrected to `.dev/ccache/`.

### Cause classification

- Confirmed for this run: a cold build compiled 624 units and spent 699.798s
  before successful link/publication, according to the supplied trace.
- Confirmed for this run: the visible `player-animation.cpp` label did not
  mean the build was stuck there; object output and later link timestamps show
  progress.
- Likely observability weakness: a “current file” status can outlive that
  file's actual compile while the worker moves through other work or link.
  Existing source prints compiler/link lines with flushing and writes
  `build/progress.json`; the user-pasted status does not establish exactly
  which reader or daemon held the stale value.
- Confirmed cache concern: supplied statistics show no hits in that run.
  Not established: whether the lane process had the reported shell ccache
  configuration, whether the cache had compatible keys, or whether ccache
  errors caused individual bypasses.
- Not established: that high priority or job count caused the full rebuild.
  The trace explicitly classified it cold with no existing executable; source
  and dependency invalidation determine rebuild scope.

### Corrected direction and remaining proof

- Keep one persistent dev-loop as the sole EXE builder and one active
  machine-wide EXE slot. Agents edit and observe; they never invoke build
  scripts, compilers, or ccache directly.
- Keep all logical processors available and the Windows worker at High
  priority, immediately below real-time, while preserving the one-build limit.
- Share only the content-addressed cache across lane worktrees. Keep object
  files, PCH, binaries, module generations, and journals local to each lane.
- EXE workers and module workers now set the same coordinator cache directory
  and lane-root path normalization. Verify with a lane build followed by an
  identical-source build in another lane and inspect the worker's own cache
  stats; a shell `ccache -p` is not sufficient evidence.
- When a build looks stuck, compare progress heartbeat, object timestamps,
  phase, process activity, and final status. A remembered filename alone is
  not a hang diagnosis.
- Scan new lane workflows for absolute checkout/temp/cache paths. Keep only
  machine dependency fallbacks (compiler/GLFW) configurable and separate from
  lane build roots; do not bake lane-specific paths into source.
- Preserve the distinction between build priority/jobs (elapsed time) and
  module/hot-reload boundaries (how often EXE rebuilds are needed).

Source/build evidence: the module worker cache setup and documentation path
correction are present in source. No build was run under the agent build
authority rule, so the new module-worker path and cross-lane cache reuse are
not runtime-verified.
