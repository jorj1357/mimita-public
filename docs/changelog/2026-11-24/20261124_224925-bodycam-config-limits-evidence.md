# Config limits → warning-first: bodycam evidence slice (issue-9e73745d3154)

Date: 2026-11-24 22:49 UTC
Agent: bodycam-spec-implementation
Context: ctx-41260d9f9184 (high)
Issue: issue-9e73745d3154 — "Replace arbitrary configuration limits with
warnings and best-effort evaluation"

Read-only activation. No source edit, build, or runtime run. The wake requested
an agent issue so that user-set values are no longer rejected; triage already
opened issue-9e73745d3154 and linked this context, so this activation added
owner-level evidence instead of duplicating that issue.

## What changed

- Appended a bodycam/camera evidence note to issue-9e73745d3154 (issue
  discussion doc). It records the upstream reject path, the full
  `presentation.bodycam`/lens/wall bound inventory, best-effort semantics for
  `-1/0.01/0.5/2/999999`, the spec conflict, and separate throw-based
  rejection owners.
- Sent one bounded, issue-linked question to `hot-reload-latency-root` (current
  owner of `config-warning-first-audit`) asking whether to keep the whole audit
  or hand the bodycam slice to this agent. No code edit while their audit is in
  flight.
- Acknowledged context `ctx-41260d9f9184`.

## Key evidence (direct source read)

- Rejection is per whole gamemode file: `validateSchema` compares numbers to
  masterlist `min`/`max` (`src/gamemode/gamemode.cpp:124-131`);
  `validateFileJson` collects errors (`:233-283`); `loadFile` rejects and keeps
  the previous mode (`:433-438`), reported by `reportInvalidFile` (`:306-342`).
  Bounds source: `config/gamemodes/masterlist/gamemode-masterlist.json`.
- Bodycam response seconds are clamped `[0.01,2.0]` at
  `src/gamemode/gamemode.cpp:719-730` and again at `src/camera.cpp:249-251`;
  masterlist bounds at lines 681-716. User reference values 0.01 and 0.5 are
  accepted; negative or 999999 is rejected by the masterlist validator first.
- Spec conflict: `docs/specs/gamemodes/bodycam.md:40-41,145` currently requires
  clamped/bounded values, so accepting arbitrary values needs a spec and
  masterlist change before code.
- Throw-based rejection owners elsewhere: `src/config/camera-config.cpp:82,92`,
  `collision-config.cpp:29-103`, `actor-collision-config.cpp:251-307`,
  `material-config.cpp:101`.

## Evidence status

- Source evidence: direct `file:line` reads above.
- Runtime evidence: none; no dev-loop run, build, or test was requested.
- Behavior proof for this issue still needs a pinned dev-loop journal with
  negative / 999999 config inputs.

## Next step

Resolve ownership with `hot-reload-latency-root`. If the bodycam slice is
delegated, implement warning-first evaluation for the bodycam fields only after
the spec/masterlist bound decision, then prove it with a pinned runtime journal.
