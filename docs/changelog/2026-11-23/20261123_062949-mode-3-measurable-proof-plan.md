# Mode 3 measurable-proof plan and live resource baseline

**Date:** 2026-11-23 UTC
**Issue:** `issue-136d4553ff92`
**User context:** `ctx-ab0aec3f6cc8`

## Work recorded

- Appended a measurable-proof execution plan to
  `docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md`
  under `2026-11-23T06:28:15.960Z — live-cpu-trace`.
- Stated the current state: Mode 1 save-triggered builds are still active
  (`devscripts/dev-loop.py:3470`); Mode 3 exists only as specification. There is
  no attempt-keyed whole-machine resource sidecar today; `devscripts/perf-watch.py`
  samples one PID and system memory but not whole-machine CPU%, is not tied to a
  build attempt, and is not started by the dev-loop.
- Proposed numeric acceptance thresholds for "negligible between-build pressure",
  bounded build burst, attempt counts, and edit-to-evidence p50/p95 per change
  class.
- Listed the required measurement deliverables: an attempt-keyed resource CSV in
  `.dev/build-timings/`, whole-machine and per-role CPU/RAM/paging/disk sampling,
  per-role separation of MiMITA, compiler/linker children, dev-loop, agent
  runtimes, Defender, and OS, and explicit-unknown handling for missing counters.
- Added the matched Mode 1 vs Mode 3 A/B checklist, including screening and p95
  trial counts and the per-trial metrics to record.
- Did not edit `docs/specs/dev-loop/dev-loop.md` or
  `docs/specs/dev-loop/build-time-observability.md`; those working-tree edits are
  leased by `cpp-latency-pressure-focus`. Sent them
  `msg-583ce3a15642` to fold the thresholds and tool gap into the spec.

## Evidence

Direct short samples only, not proof, taken during live run
`20261123_055730_01` (mode `jugger_horde`, server PID 8804 and client PID 3600
alive, no build running) on an i5-13420H host with 12 logical / 8 physical cores
and 15.6 GB RAM:

- Whole-machine CPU: `Get-Counter` 5x1 s average 94.2%, maximum 99.6%.
- Free physical RAM: 2.5 GB.
- Approximate one-core percentages: `mimita` ~208 (client + server, ~17% of 12
  logical CPUs), `MsMpEng`/Defender ~125, each visible `opencode` instance
  ~25-28 across several processes, `pythonw` dev-loop ~23.

The sample shows the current pressure is not created by a build; it comes from
the live game, Defender, and the agent fleet. This is why the plan requires
subtracting those roles before calling between-build pressure negligible.

## Verification

- `git diff --check` passes for the issue edit after normalizing the appended
  block to LF.
- No source, build configuration, spec owned by another agent, running process,
  build, or runtime test was changed.

## Limitations

- The resource baseline is a short observation, explicitly not proof, and must be
  repeated with the attempt-keyed sidecar before any Mode 3 comparison claim.
- Mode 3 remains proposed and unimplemented; the plan does not implement the
  batch gate or the resource sidecar.
