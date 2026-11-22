# CPU-claims context review (ctx-20f2a628316f)

## Session

- Owner: `issue-review-sweeper`
- Focus: review high-priority user context `ctx-20f2a628316f` and its linked issue
  `issue-ed3ac775283d` ("Measure and reduce MiMITA CPU use during idle and normal play").
- Summary: verified routing, extended the historical "claiming cores" audit with the
  earliest and still-live build-policy change, appended the evidence to the issue, and
  acknowledged the context. No source, affinity, priority, build, or runtime change was made.

## User request

> i think it was somejing else like some days ago changing cpu usage and claiming cores can u investgate

Routing was already correct: the context is linked as the primary context of
`issue-ed3ac775283d`, owned by `cpu-build-efficiency`, with one open task
(`server-phase-budget-log-summary`). No ownership repair was needed.

## Findings appended to the issue

- No persistent CPU affinity exists: repository search found no
  `SetProcessAffinityMask`, `SetThreadAffinityMask`, or `sched_setaffinity`
  implementation, and sampled MiMITA processes had Normal priority with affinity
  mask `0xFFF` (all 12 logical CPUs allowed, none reserved).
- The literal "claim all cores at high priority" change is older than the
  `20261122_023300` audit reported. On 2026-11-09, commits `b7516a78` and
  `ed8787fe` changed the EXE build from a 4-job / half-core cap at
  `BELOW_NORMAL_PRIORITY_CLASS` to all logical processors (`os.cpu_count()`) at
  `HIGH_PRIORITY_CLASS`. The policy is still live at `build.py:125` and
  `devscripts/dev_loop/build.py:95`, and the added spec text says "never a fixed
  four-job cap" and "do not lower the build to below normal priority".
- The later build-worker sequence is also real: `d4898e0b` (2026-11-19) made the
  game-DLL worker `--jobs 0` mean `os.cpu_count()` (12); `d89884f6` (2026-11-21)
  changed the caller to explicit `--jobs 10`, still `--priority high`.
- MiMITA game processes do not reserve cores, but `maxFrames=999` with VSync off
  lets an active client/server hold multiple cores (2026-11-21 samples: client
  ~298% of one core, server ~102%). This is visible-play work, not an affinity claim.

## Conclusion

The user's memory most plausibly maps to the 2026-11-09 build policy change
(all cores at high priority) plus the 2026-11-19/21 DLL-worker job count, not to any
game-process core reservation. This is a build/priority tradeoff, not proof that the
build worker causes the reported idle CPU use.

## Next discriminating measurement

Run the same cache/source incremental build with jobs capped at 4-6 and normal
priority against the current all-cores high-priority path; record compile/link wall
time and concurrent client FPS/CPU. Do not change affinity or priority until that
comparison shows a benefit and the interactive tradeoff is accepted.

## Not proven

No runtime behavior changed or was validated. The idle/background CPU target and the
active-gameplay CPU/frame-time tradeoff remain open under `issue-ed3ac775283d`.
