# Build, process, event, module, hit-volume, and editor specifications

- Task/agent: `spec-event-build-volumes`
- Date, time, timezone: 2026-11-27T19:10Z, ISO 8601
- Status: documentation only; no source, build, or runtime change
- Linked issue: `issue-2d4032ac8044` (related; no issue mutation in this session)

## Requested behavior

- Make build requests the seal of work that is already known to be healthy.
- Add cheap pre-build checks for syntax, header fan-out, link symbols, time
  estimate, and resource admission.
- Make builds fast enough for sub-10-second hot-module reloads and low enough
  resource use for many concurrent agent runtimes.
- Move toward separate hot-reload module DLLs and one-header-one-concept
  headers with minimal includes and a fan-out budget.
- Make every MiMITA process prove why it is alive and name the event that
  releases it, instead of being killed on a timer.
- Put agent-run windows in the taskbar with run/mode/issue identity, and stop
  unseen headless clients from producing audio.
- Broadcast every user-visible agent statement to all active agents; store user
  context once.
- Extend event-driven coordination to processes, builds, checks, and the whole
  runtime, including the 60 Hz tick as an event cascade.
- Specify one `ActorHitVolume` folded into one collision surface for hitscan,
  projectile, melee, splash, and server claim validation.
- Fold the animation editor and weapon pose editor into the map editor.

## Files added

| File | Purpose |
|---|---|
| `docs/specs/dev-loop/spec-authoring.md` | Magic-wand doctrine, required structure, evidence classes, gap recording |
| `docs/specs/dev-loop/build-preflight-and-admission.md` | C0–C5 check ladder, check record, build gate, admission |
| `docs/specs/dev-loop/build-performance-budget.md` | Time/resource budgets, profiles, memory-aware jobs, outlier splits |
| `docs/specs/dev-loop/hot-modules-and-header-boundaries.md` | Resident boundary, module map, header ownership, fan-out ratchet |
| `docs/specs/dev-loop/process-lifecycle-and-release-events.md` | Lease, release events, taskbar/visibility, audio policy, job objects |
| `docs/specs/dev-loop/event-driven-runtime.md` | Event envelope, tick-as-event, nodes/connections, migration phases |
| `docs/specs/dev-loop/agent-broadcast-and-shared-context.md` | One broadcast model, user context once, questions/replies |
| `docs/specs/networking/visible-hit-volumes.md` | One actor hit volume, one collision surface, server claim verification |
| `docs/specs/animations/animation-and-weapon-pose-editor.md` | Map-editor animation/weapon pose modes, overrides, shared pose |

## Files extended

- `docs/specs/dev-loop/event-driven-coordination.md`: process/check/broadcast
  events and no-polling implementation rules.
- `docs/specs/dev-loop/agent-runtime-registry.md`: lease v2 and window/audio
  policy.
- `docs/specs/dev-loop/system-resource-measurement.md`: client frame-cap
  baseline and per-client CPU budget.
- `docs/specs/networking/contact-authority.md`: pointer to the visible hit
  volume contract.
- `docs/specs/animations/animation-editor.md`: fold-in scope pointer.
- `docs/ROUTER.md`: routes for the new specs.

## Measured evidence used

- Generation 804 build: 192 s total; `recast-navigation.cpp` 123.5 s; lld link
  46.2 s for 646 objects / 506 MB into a 237 MB DLL; publish 0.5 s.
- Earlier generation 640: one changed TU 95.9 s and lld 11.4 s; generation 634:
  55 compiled, lld 15.4 s.
- Header fan-in: `pch.h` 636; `structured-log.h` 360; `physics-types.h` 312;
  `movement-types.h` 292; `map-mesh-types.h` 287; `weapon-types.h` 286;
  `config.h` 286; `avatar-rig.h` 280; `actor-body.h` 272.
- Source ownership: 626 game, 42 resident, 3 bootstrap, 25 unknown/stale.
- Client frame pacing: `config/video-settings.json` `maxFrames: 999`; one
  journal reported `avg_frame_ms 3.55` (~282 FPS), roughly 0.85–1.0 logical
  core per client.
- `Audio::AudioUpdate` steady-state median under 1 ms; 100–270 ms first-play
  decode spikes on the main thread.
- Isolated/parallel test runs set `MIMITA_AGENT_HEADLESS_GUI=1`; the renderer
  creates the window hidden and never shows it, so it has no taskbar entry.
  Audio is not disabled for those clients.

## Validation

- Documentation review: purpose blocks and non-goals present in every new spec.
- Relative-link check over the new and edited specs: no broken relative links.
- No C++ source, config, build, executable, runtime, or coordination authority
  was changed.
- No build or runtime proof is claimed. This session is specified-only.

## Follow-ups (not implemented here)

- Add the `check` request, check record, and build gate.
- Add `process-guard`, release events, Job Objects, taskbar policy, and audio
  policy.
- Add the client frame cap and audio decode worker.
- Split `recast-navigation.cpp`; add the fast build profile and hot modules.
- Enforce header budgets and include lint.
- Implement `ActorHitVolume` and server claim verification.
- Implement the map-editor animation/weapon pose modes.

## Pre-existing changes

The shared worktree was already dirty at session start with other agents'
source, config, issue, and changelog work. This session added and edited only
the specification and router files listed above.

## Addendum: compiler cache policy

A follow-up toolchain check confirmed the actual paths:

- `C:\important\msys64\mingw64\bin\g++.exe`
- `C:\important\msys64\mingw64\bin\ld.lld.exe`
- `C:\ProgramData\chocolatey\bin\ccache.exe`

The worker can prepend `ccache`, and `dev_loop/build.py` sets
`CCACHE_DIR=.dev/ccache`. `docs/specs/dev-loop/build-performance-budget.md` now
requires a measured hit-rate and wall-time win before `ccache` stays enabled,
records the decision in the postmortem, and keeps direct agent invocation
forbidden. No code or build change was made.

## Handoff

`docs/handoffs/2026-11-27-build-process-event-implementation-handoff.md` hands
items 1–7 (measurement, frame cap/taskbar/audio, process guard, check gate, fast
build, header budgets, hot modules) to the next agent. Items 8–9 (visible hit
volumes, animation/weapon editor) are deferred until 1–7 are measured. The next
agent was not launched by this session; the doc includes the exact wake command.
