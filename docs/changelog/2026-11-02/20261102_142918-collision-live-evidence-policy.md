Time created: 2026-11-02T14:29:18Z
Time last updated: 2026-11-02T14:29:18Z

Purpose:
Record the requested documentation direction for always-on collision
monitoring, natural live gameplay, human-recorded collision routes, event-driven
completion, map coverage, and dev-loop activity visibility.

This is a documentation-only design update. It does not implement monitoring,
change scenario playback, build a GUI, change collision behavior, or claim
runtime or human acceptance.

## Session context

- Branch: `20261011zt`
- HEAD at start: `20578d11`
- Pre-existing working-tree edits were present in account, analytics, audio,
  gamemode, video, coordinator/networking, website, persistence, regression,
  router, open-gameplay-issues, and Blender submodule files. They were left
  unchanged. In particular, the user's existing edits in
  `docs/specs/open-gameplay-issues.md` were preserved.
- User direction: ordinary live gameplay and `events.jsonl` should be the main
  way to discover unanticipated collision failures; human-recorded routes
  should repeat known failures; AI-authored fixed routes should not be the
  primary proof; keep per-tick detection useful without flooding logs; complete
  checks from runtime events rather than fixed durations; rotate other maps and
  run all supported maps for collision-wide changes; modernize the terminal
  dev-loop view into a native Python window with an activity feed.

## Documentation changes

- `docs/specs/debug-logging/debug-logging.md`, “Always-on collision
  monitoring”: replaced the idea that collision evidence requires per-tick
  trace output with always-on inexpensive fixed-tick checks, bounded recent
  history, anomaly-triggered structured events, cached broadphase/reusable
  scratch requirements, and targeted high-volume Trace capture.
- `docs/workflows/runtime-scenario-validation.md`, “Test policy” and “Recorded
  regressions and optional focused checks”: made unscripted real gameplay the
  discovery path, human-recorded routes the focused repeat path, added
  `NOT_COVERED`, map rotation/full-map collision coverage, background long
  checks, and event-driven completion without a scenario-level timeout.
  Removed the unconditional requirement for a fixed minimum scenario suite.
- `docs/specs/dev-loop/scenario-input-recording.md`, “Collision re-simulation
  requirement”: documented that current schema 2 transform-follow proxies do
  not re-run NPC movement/collision, and specified the evidence needed for
  human-recorded per-tick collision re-simulation.
- `docs/specs/dev-loop/quality-gates.md`, “Event-driven scenario completion”:
  removed elapsed time as the gameplay verdict and specified completion on the
  final event/result, with pending state until explicit stop or process failure.
- `docs/specs/dev-loop/dev-loop.md`, “Native dashboard and contributor
  activity feed — desired”: captured the user's request for a native Python
  window, live MiMITA status, observable AI/contributor task/file/build/check
  activity, separation of work status from runtime proof, and modular Python
  components.

The key distinction recorded is: cheap checks may run every simulation tick,
but the journal should emit rich events on meaningful transitions and preserve
a bounded recent window. Human-recorded routes are repeatable checks, not proof
of broad behavior; a route that did not reach the geometry is not a pass.

## Validation and review

- Read the routed logging specification, runtime validation workflow, scenario
  recording specification, dev-loop quality-gate specification, and
  documentation-checker skill.
- Compared the current scenario recording contract with the new statement:
  schema 2 server playback follows NPC/remote-player transforms and disables
  their normal AI.
- `git diff --check` found only pre-existing trailing whitespace in the user's
  modified `docs/specs/open-gameplay-issues.md`; no whitespace issue was
  introduced in the files changed by this session.
- No build, test, game launch, or scenario was run because this session changed
  documentation only and the user requested investigation/design, not runtime
  changes.
- Documentation TODO review found the directly relevant existing note
  `docs/architecture/player-npc-systems/npc-movement.md:22`:
  `9 8 2026 1019 est jorj todo - explain`. Suggested expansion: explain the
  NPC goal-to-route-to-shared-movement ownership and its runtime evidence path.
  It was not changed as it is outside this documentation update.

## Remaining work and acceptance

Implementation still needs a separate approved task: add and measure the
bounded per-tick monitors, determine which collision checks can safely run per
actor tick, improve collision re-simulation for recorded NPC inputs, wire
event-driven run completion, and build the native dashboard/activity feed.
Human live-play confirmation remains necessary for collision behavior.
